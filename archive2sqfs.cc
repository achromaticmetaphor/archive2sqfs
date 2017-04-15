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

#define _POSIX_C_SOURCE 200809L

#include <cstring>
#include <memory>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static int usage(char const * const progname)
{
  fprintf(stderr, "usage: %s [--strip N] outfile [infile]\n", progname);
  return 3;
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

int main(int argc, char * argv[])
{
  if (argc < 2 || argc > 5)
    return usage(argv[0]);

  char const * const outfilepath = argv[argc <= 3 ? 1 : 3];
  char const * infilepath = argc == 3 || argc == 5 ? argv[argc - 1] : nullptr;
  size_t strip = 0;

  if (argc > 3)
    if (!std::strcmp("--strip", argv[1]))
      strip = strtoll(argv[2], nullptr, 10);
    else
      return usage(argv[0]);

  if (!access(outfilepath, F_OK))
    {
      fprintf(stderr, "ERROR: output file exists: %s\n", outfilepath);
      return 3;
    }

  struct sqsh_writer writer(outfilepath);
  dirtree_dir rootdir(&writer);

  FILE * const infile = infilepath == nullptr ? stdin : fopen(infilepath, "rb");
  struct archive * const archive = infile == nullptr ? nullptr : archive_read_new();

  bool failed = archive == nullptr;
  failed = failed || archive_read_support_filter_all(archive) != ARCHIVE_OK;
  failed = failed || archive_read_support_format_all(archive) != ARCHIVE_OK;
  failed = failed || archive_read_open_FILE(archive, infile) != ARCHIVE_OK;

  size_t const block_size = (size_t) 1 << writer.super.block_log;
  unsigned char buff[block_size];
  struct archive_entry * entry;

  while (!failed && archive_read_next_header(archive, &entry) == ARCHIVE_OK)
    {
      char const * const pathname = strip_path(strip, archive_entry_pathname(entry));
      auto const filetype = archive_entry_filetype(entry);
      auto const mode = archive_entry_perm(entry);
      auto const uid = archive_entry_uid(entry);
      auto const gid = archive_entry_gid(entry);
      auto const mtime = archive_entry_mtime(entry);
      switch (filetype)
        {
          case AE_IFDIR:
            {
              auto dir = rootdir.subdir_for_path(pathname);
              dir->mode = mode;
              dir->uid = uid;
              dir->gid = gid;
              dir->mtime = mtime;
            }
            break;

          case AE_IFREG:
            {
              auto reg = new dirtree_reg(&writer, mode, uid, gid, mtime);
              rootdir.put_file(pathname, reg);

              int64_t i;
              for (i = archive_entry_size(entry); i >= block_size && !failed; i -= block_size)
                {
                  failed = failed || archive_read_data(archive, buff, block_size) != block_size;
                  failed = failed || reg->append(buff, block_size);
                }
              if (i > 0 && !failed)
                {
                  failed = failed || archive_read_data(archive, buff, i) != i;
                  failed = failed || reg->append(buff, i);
                }

              failed = failed || reg->flush();
            }
            break;

          case AE_IFLNK:
            rootdir.put_file(pathname, new dirtree_sym(&writer, archive_entry_symlink(entry), mode, uid, gid, mtime));
            break;

          case AE_IFBLK:
            rootdir.put_file(pathname, new dirtree_dev(&writer, SQFS_INODE_TYPE_BLK, archive_entry_rdev(entry), mode, uid, gid, mtime));
            break;

          case AE_IFCHR:
            rootdir.put_file(pathname, new dirtree_dev(&writer, SQFS_INODE_TYPE_CHR, archive_entry_rdev(entry), mode, uid, gid, mtime));
            break;

          case AE_IFSOCK:
            rootdir.put_file(pathname, new dirtree_ipc(&writer, SQFS_INODE_TYPE_SOCK, mode, uid, gid, mtime));
            break;

          case AE_IFIFO:
            rootdir.put_file(pathname, new dirtree_ipc(&writer, SQFS_INODE_TYPE_PIPE, mode, uid, gid, mtime));
            break;

          default:
            failed = true;
        }
    }

  failed = failed || rootdir.write_tables();
  failed = failed || writer.write_header();

  if (archive != nullptr)
    archive_read_free(archive);

  if (infile != nullptr)
    fclose(infile);

  return failed;
}
