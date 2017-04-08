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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"

static inline size_t rup(size_t const a, size_t const s)
{
  size_t const mask = ~(SIZE_MAX << s);
  size_t const factor = (a >> s) + ((a & mask) != 0);
  return ((size_t) 1 << s) * factor;
}

static void mdw_write_block_compressed(struct mdw * const mdw, size_t const block_len, unsigned char const * const block, uint16_t const bsize)
{
  uint8_t tmp[2];
  le16(tmp, bsize);
  mdw->table.push_back(tmp[0]);
  mdw->table.push_back(tmp[1]);

  for (std::size_t i = 0; i < block_len; ++i)
    mdw->table.push_back(block[i]);
}

void mdw_write_block_no_pad(struct mdw * const mdw)
{
  if (mdw->buff_pos == 0)
    return;

  unsigned long int zsize = compressBound(mdw->buff_pos);
  unsigned char zbuff[zsize];
  compress2(zbuff, &zsize, mdw->buff, mdw->buff_pos, 9);

  bool const compressed = zsize < mdw->buff_pos;
  unsigned char * const buff = compressed ? zbuff : mdw->buff;
  size_t const size = compressed ? zsize : mdw->buff_pos;

  mdw->buff_pos = 0;
  mdw_write_block_compressed(mdw, size, buff, compressed ? size : (size | SQFS_META_BLOCK_COMPRESSED_BIT));
}

void mdw_write_block(struct mdw * const mdw)
{
  memset(mdw->buff + mdw->buff_pos, 0, SQFS_META_BLOCK_SIZE - mdw->buff_pos);
  mdw->buff_pos = SQFS_META_BLOCK_SIZE;
  mdw_write_block_no_pad(mdw);
}

uint64_t mdw_put(struct mdw * const mdw, unsigned char const * b, size_t len)
{
  uint64_t const addr = meta_address(mdw->table.size(), mdw->buff_pos);

  while (len != 0)
    {
      size_t const remaining = SQFS_META_BLOCK_SIZE - mdw->buff_pos;
      size_t const added = len > remaining ? remaining : len;

      memcpy(mdw->buff + mdw->buff_pos, b, added);
      mdw->buff_pos += added;
      if (mdw->buff_pos == SQFS_META_BLOCK_SIZE)
        mdw_write_block(mdw);

      len -= added;
      b += added;
    }

  return addr;
}