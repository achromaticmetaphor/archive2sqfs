/*
Copyright (C) 2016  Charles Cagle

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

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

int main(int argc, char * argv[])
{
  if (argc != 2 && argc != 3)
    {
      fprintf(stderr, "usage: %s outfile [infile]\n", argv[0]);
      return 3;
    }

  struct sqsh_writer writer;
  _Bool const writer_failed = sqsh_writer_init(&writer, argv[1], SQFS_BLOCK_LOG_DEFAULT);
  struct dirtree * const root = writer_failed ? NULL : dirtree_dir_new(&writer);

  FILE * const infile = root == NULL ? NULL : argc == 3 ? fopen(argv[2], "rb") : stdin;
  struct archive * const archive = infile == NULL ? NULL : archive_read_new();

  _Bool failed = archive == NULL;
  failed = failed || archive_read_support_filter_all(archive) != ARCHIVE_OK;
  failed = failed || archive_read_support_format_all(archive) != ARCHIVE_OK;
  failed = failed || archive_read_open_FILE(archive, infile) != ARCHIVE_OK;

  size_t const block_size = writer_failed ? (size_t) 0 : (size_t) 1 << writer.super.block_log;
  unsigned char buff[block_size];
  struct archive_entry * entry;

  while (!failed && archive_read_next_header(archive, &entry) == ARCHIVE_OK)
    {
      char const * pathname = archive_entry_pathname(entry);
      struct dirtree * dt = NULL;
      mode_t const filetype = archive_entry_filetype(entry);
      switch (filetype)
        {
          case AE_IFDIR:
            dt = dirtree_get_subdir_for_path(&writer, root, pathname);
            failed = failed || dt == NULL;
            break;
          case AE_IFREG:
            dt = dirtree_put_reg_for_path(&writer, root, pathname);
            failed = failed || dt == NULL;

            int64_t i;
            for (i = archive_entry_size(entry); i >= block_size && !failed; i -= block_size)
              {
                failed = failed || archive_read_data(archive, buff, block_size) != block_size;
                failed = failed || dirtree_reg_append(&writer, dt, buff, block_size);
              }
            if (i > 0 && !failed)
              {
                failed = failed || archive_read_data(archive, buff, i) != i;
                failed = failed || dirtree_reg_append(&writer, dt, buff, i);
              }

            failed = failed || dirtree_reg_flush(&writer, dt);
            break;
          default:;
        }

      if (dt != NULL)
        {
          dt->mode = archive_entry_perm(entry);
          dt->uid = archive_entry_uid(entry);
          dt->gid = archive_entry_gid(entry);
          dt->mtime = archive_entry_mtime(entry);
        }
    }

  failed = failed || dirtree_write_tables(&writer, root);
  failed = failed || sqsh_writer_write_header(&writer);
  failed = failed || sqsh_writer_destroy(&writer);

  if (failed)
    remove(argv[1]);

  if (archive != NULL)
    archive_read_free(archive);

  if (infile != NULL)
    fclose(infile);

  if (root != NULL)
    dirtree_free(root);

  return failed;
}
