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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <zlib.h>

#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"

static void mdw_write_block_compressed(struct mdw * const mdw, size_t const block_len, unsigned char const * const block, uint16_t const bsize)
{
  if (mdw->block + block_len + 2 > mdw->table_len)
    {
      do
        mdw->table_len += 0x100000;
      while (mdw->block + block_len + 2 > mdw->table_len);
      mdw->table = g_realloc(mdw->table, mdw->table_len);
    }

  le16(mdw->table + mdw->block, bsize);
  memcpy(mdw->table + mdw->block + 2, block, block_len);
  mdw->block += block_len + 2;
}

void mdw_write_block_no_pad(struct mdw * const mdw)
{
  unsigned long int zsize = compressBound(mdw->buff_pos);
  unsigned char zbuff[zsize];
  compress2(zbuff, &zsize, mdw->buff, mdw->buff_pos, 9);

  _Bool const compressed = zsize < mdw->buff_pos;
  unsigned char * const buff = compressed ? zbuff : mdw->buff;
  size_t const size = compressed ? zsize : mdw->buff_pos;

  mdw_write_block_compressed(mdw, size, buff, compressed ? size : (size | SQFS_META_BLOCK_COMPRESSED_BIT));
  mdw->buff_pos = 0;
}

void mdw_write_block(struct mdw * const mdw)
{
  memset(mdw->buff + mdw->buff_pos, 0, SQFS_META_BLOCK_SIZE - mdw->buff_pos);
  mdw->buff_pos = SQFS_META_BLOCK_SIZE;
  mdw_write_block_no_pad(mdw);
}

uint64_t mdw_put(struct mdw * const mdw, unsigned char const * const b, size_t const len)
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
