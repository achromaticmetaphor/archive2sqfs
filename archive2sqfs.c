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

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <search.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>
#include <glib.h>
#include <zlib.h>

#include "dirtree.h"
#include "le.h"
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
  _Bool const writer_failed = sqsh_writer_init(&writer, argv[1]);
  struct dirtree * const root = writer_failed ? NULL : dirtree_dir_new(&writer);

  FILE * const infile = root == NULL ? NULL : argc == 3 ? fopen(argv[2], "rb") : stdin;
  struct archive * const archive = infile == NULL ? NULL : archive_read_new();

  if (archive != NULL)
    {
      archive_read_support_filter_all(archive);
      archive_read_support_format_all(archive);
    }
  int const archive_opened = archive == NULL ? ARCHIVE_FATAL : archive_read_open_FILE(archive, infile);

  if (archive_opened == ARCHIVE_OK)
    {
      struct archive_entry * entry;
      while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
        {
          char const * pathname = archive_entry_pathname(entry);
          if (archive_entry_filetype(entry) == AE_IFDIR)
            dirtree_get_subdir_for_path(&writer, root, pathname);
        }
    }

  dirtree_write_tables(&writer, root);
  sqsh_writer_write_header(&writer);

  if (archive != NULL)
    archive_read_free(archive);

  if (infile != NULL)
    fclose(infile);

  dirtree_free(root);
  sqsh_writer_destroy(&writer);

  return archive_opened == ARCHIVE_OK ? 0 : 1;
}
