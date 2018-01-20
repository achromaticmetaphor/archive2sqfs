/*
Copyright (C) 2016, 2017  Charles Cagle

This file is part of archive2sqfs.

archive2sqfs is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3.

archive2sqfs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with archive2sqfs.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include "archive_reader.h"
#include "compressor.h"
#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static int usage(std::string const & progname)
{
  std::cerr << "usage: " << progname << " [--strip=N] [--compressor=<zlib|none>] outfile [infile]" << std::endl;
  return EINVAL;
}

static char const * strip_path(size_t const strip, char const * pathname)
{
  for (size_t i = 0; i < strip; i++)
    {
      char const * const sep = std::strchr(pathname, '/');
      if (sep == nullptr)
        break;
      pathname = sep + 1;
    }

  return pathname;
}

static bool begins_with(std::string const & s, std::string const & prefix)
{
  return prefix.size() <= s.size() && prefix.compare(0, prefix.size(), s, 0, prefix.size()) == 0;
}

template <typename C>
static bool proc_prefix_arg(std::string const & prefix, std::string const & arg, C act)
{
  bool const matched = begins_with(arg, prefix);
  if (matched)
    act(arg.substr(prefix.size()));
  return matched;
}

int main(int argc, char * argv[])
{
  std::size_t strip = 0;
  int block_log = SQFS_BLOCK_LOG_DEFAULT;
  std::string compressor = COMPRESSOR_DEFAULT;

  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    if (proc_prefix_arg("--strip=", argv[i], [&](auto s) { strip = strtoll(s.data(), nullptr, 10); }))
      ;
    else if (proc_prefix_arg("--compressor=", argv[i], [&](auto s) { compressor = s; }))
      ;
    else
      args.push_back(argv[i]);

  if (args.size() < 1 || args.size() > 2)
    return usage(argv[0]);

  struct sqsh_writer writer(args[0], block_log, compressor);
  archive_reader archive = args.size() > 1 ? archive_reader(args[1]) : archive_reader(stdin);
  std::shared_ptr<dirtree_dir> rootdir = dirtree_dir::create_root_dir(&writer);
  int64_t const block_size = 1 << writer.super.block_log;
  bool failed = false;

  while (!failed && archive.next())
    {
      char const * const pathname = strip_path(strip, archive_entry_pathname(archive.entry));
      auto const filetype = archive_entry_filetype(archive.entry);
      auto const mode = archive_entry_perm(archive.entry);
      auto const uid = archive_entry_uid(archive.entry);
      auto const gid = archive_entry_gid(archive.entry);
      auto const mtime = archive_entry_mtime(archive.entry);
      switch (filetype)
        {
          case AE_IFDIR:
            {
              auto dir = rootdir->subdir_for_path(pathname);
              dir->mode = mode;
              dir->uid = uid;
              dir->gid = gid;
              dir->mtime = mtime;
            }
            break;

          case AE_IFREG:
            {
              std::vector<unsigned char> buff;
              auto reg = std::make_shared<dirtree_reg>(&writer, mode, uid, gid, mtime);
              rootdir->put_file(pathname, reg);

              int64_t i;
              for (i = archive_entry_size(archive.entry); i >= block_size && !failed; i -= block_size)
                {
                  failed = failed || archive.read(buff, block_size) != block_size;
                  reg->append(buff);
                }
              if (i > 0 && !failed)
                {
                  failed = failed || archive.read(buff, i) != i;
                  reg->append(buff);
                }

              reg->flush();
            }
            break;

          case AE_IFLNK:
            rootdir->put_file(pathname, std::make_shared<dirtree_sym>(&writer, archive_entry_symlink(archive.entry), mode, uid, gid, mtime));
            break;

          case AE_IFBLK:
            rootdir->put_file(pathname, std::make_shared<dirtree_dev>(&writer, SQFS_INODE_TYPE_BLK, archive_entry_rdev(archive.entry), mode, uid, gid, mtime));
            break;

          case AE_IFCHR:
            rootdir->put_file(pathname, std::make_shared<dirtree_dev>(&writer, SQFS_INODE_TYPE_CHR, archive_entry_rdev(archive.entry), mode, uid, gid, mtime));
            break;

          case AE_IFSOCK:
            rootdir->put_file(pathname, std::make_shared<dirtree_ipc>(&writer, SQFS_INODE_TYPE_SOCK, mode, uid, gid, mtime));
            break;

          case AE_IFIFO:
            rootdir->put_file(pathname, std::make_shared<dirtree_ipc>(&writer, SQFS_INODE_TYPE_PIPE, mode, uid, gid, mtime));
            break;

          default:
            failed = true;
        }
    }

  failed = failed || archive.fail;
  failed = failed || writer.finish_data();
  failed = failed || rootdir->write_tables();
  failed = failed || writer.write_header();

  return failed;
}
