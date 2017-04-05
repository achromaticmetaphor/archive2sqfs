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

#ifndef LSL_MDW_H
#define LSL_MDW_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "sqsh_defs.h"

struct mdw
{
  size_t block;
  size_t buff_pos;
  size_t table_len;
  unsigned char * table;
  unsigned char buff[SQFS_META_BLOCK_SIZE];
};

static inline void mdw_init(struct mdw * const mdw)
{
  mdw->block = 0;
  mdw->buff_pos = 0;
  mdw->table_len = 0;
  mdw->table = nullptr;
}

static inline void mdw_destroy(struct mdw * const mdw)
{
  free(mdw->table);
}

static inline int mdw_out(struct mdw * const mdw, FILE * const out)
{
  return fwrite(mdw->table, 1, mdw->block, out) != mdw->block;
}

int mdw_write_block_no_pad(struct mdw *);
int mdw_write_block(struct mdw *);
uint64_t mdw_put(struct mdw *, unsigned char const *, size_t);

#endif
