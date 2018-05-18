/*
Copyright (C) 2016, 2017, 2018  Charles Cagle

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
#include <string>
#include <thread>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

#include "archive_reader.h"
#include "compressor.h"
#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

using namespace std::literals;

static int usage(std::string const & progname)
{
  std::cerr << "usage: " << progname
            << " [--single-thread] [--strip=N] [--compressor=<type>] "
               "outfile [infile]"
            << std::endl;
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
  return prefix.size() <= s.size() &&
         prefix.compare(0, prefix.size(), s, 0, prefix.size()) == 0;
}

template <typename C>
static bool proc_prefix_arg(std::string const & prefix,
                            std::string const & arg, C act)
{
  bool const matched = begins_with(arg, prefix);
  if (matched)
    act(arg.substr(prefix.size()));
  return matched;
}

int main(int argc, char * argv[])
{
  std::size_t strip = 0;
  bool single_thread = false;
  int block_log = SQFS_BLOCK_LOG_DEFAULT;
  std::string compressor = COMPRESSOR_DEFAULT;

  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    if (proc_prefix_arg("--strip=", argv[i], [&](auto s) {
          strip = strtoll(s.data(), nullptr, 10);
        }))
      ;
    else if (proc_prefix_arg("--compressor=", argv[i],
                             [&](auto s) { compressor = s; }))
      ;
    else if ("--single-thread"s == argv[i])
      single_thread = true;
    else
      args.push_back(argv[i]);

  if (args.size() < 1 || args.size() > 2)
    return usage(argv[0]);

  struct sqsh_writer writer(args[0], block_log, compressor, single_thread);
  archive_reader archive =
      args.size() > 1 ? archive_reader(args[1]) : archive_reader(stdin);
  dirtree_dir rootdir(&writer);
  int64_t const block_size = 1 << writer.super.block_log;

  while (archive.next())
    {
      auto const pathname = strip_path(strip, archive.pathname());
      switch (archive.filetype())
        {
          case AE_IFDIR:
            rootdir.subdir_for_path(pathname).update_metadata(archive);
            break;

          case AE_IFREG:
            {
              std::vector<char> buff;
              auto & reg = rootdir.put_file_with_metadata<dirtree_reg>(
                  pathname, archive);

              int64_t i;
              for (i = archive.filesize(); i >= block_size; i -= block_size)
                {
                  archive.read(buff, block_size);
                  reg.append(buff);
                }
              if (i > 0)
                {
                  archive.read(buff, i);
                  reg.append(buff);
                }

              reg.flush();
            }
            break;

          case AE_IFLNK:
            rootdir.put_file_with_metadata<dirtree_sym>(
                pathname, archive, archive.symlink_target());
            break;

          case AE_IFBLK:
            rootdir.put_file_with_metadata<dirtree_dev>(
                pathname, archive, SQFS_INODE_TYPE_BLK, archive.rdev());
            break;

          case AE_IFCHR:
            rootdir.put_file_with_metadata<dirtree_dev>(
                pathname, archive, SQFS_INODE_TYPE_CHR, archive.rdev());
            break;

          case AE_IFSOCK:
            rootdir.put_file_with_metadata<dirtree_ipc>(pathname, archive,
                                                        SQFS_INODE_TYPE_SOCK);
            break;

          case AE_IFIFO:
            rootdir.put_file_with_metadata<dirtree_ipc>(pathname, archive,
                                                        SQFS_INODE_TYPE_PIPE);
            break;
        }
    }

  bool failed = writer.finish_data();
  rootdir.write_tables();
  writer.write_header();

  return failed;
}
