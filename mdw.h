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

#ifndef LSL_MDW_H
#define LSL_MDW_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

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
  mdw->table = NULL;
}

static inline void mdw_destroy(struct mdw * const mdw)
{
  free(mdw->table);
}

static inline void mdw_write_block_compressed(struct mdw * const mdw, size_t const block_len, unsigned char const * const block, unsigned char const block_size[static 2])
{
  if (mdw->block + block_len + 2 > mdw->table_len)
    {
      do
        mdw->table_len = mdw->table_len * 2 + 1;
      while (mdw->block + block_len + 2 > mdw->table_len);
      mdw->table = g_realloc(mdw->table, mdw->table_len);
    }

  mdw->table[mdw->block] = block_size[0];
  mdw->table[mdw->block + 1] = block_size[1];
  memcpy(mdw->table + mdw->block + 2, block, block_len);
  mdw->block += block_len + 2;
}

static inline void mdw_write_block(struct mdw * const mdw)
{
  unsigned char const size[2] = {SQFS_META_BLOCK_SIZE & 0xff, ((SQFS_META_BLOCK_SIZE >> 8) & 0xff) | 0x80};
  mdw_write_block_compressed(mdw, SQFS_META_BLOCK_SIZE, mdw->buff, size);
  mdw->buff_pos = 0;
}

static inline uint64_t mdw_put(struct mdw * const mdw, unsigned char const * const b, size_t const len)
{
  // TODO WHILE
  uint64_t const addr = meta_address(mdw->block, mdw->buff_pos);
  size_t const remaining = SQFS_META_BLOCK_SIZE - mdw->buff_pos;
  size_t const added = len > remaining ? remaining : len;

  memcpy(mdw->buff + mdw->buff_pos, b, added);
  mdw->buff_pos += added;
  if (mdw->buff_pos == SQFS_META_BLOCK_SIZE)
    {
      mdw_write_block(mdw);
      size_t const left = len - added;
      memcpy(mdw->buff, b + added, left);
      mdw->buff_pos = left;
    }

  return addr;
}

static inline void mdw_out(struct mdw * const mdw, FILE * const out)
{
  fwrite(mdw->table, 1, mdw->block, out);
}

#endif
