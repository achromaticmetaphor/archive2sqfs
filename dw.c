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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zlib.h>

#include "sqsh_defs.h"

uint32_t dw_write_data(unsigned char const * const buff, size_t const len, FILE * const out)
{
  if (len == 0)
    return SQFS_BLOCK_COMPRESSED_BIT;

  unsigned long int zsize = compressBound(len);
  unsigned char zbuff[zsize];
  if (compress2(zbuff, &zsize, buff, len, 9) != Z_OK)
    return 0xffffffff;

  _Bool const compressed = zsize < len;
  unsigned char const * const block = compressed ? zbuff : buff;
  size_t const size = compressed ? zsize : len;

  if (fwrite(block, 1, size, out) != size)
    return 0xffffffff;

  return compressed ? size : (size | SQFS_BLOCK_COMPRESSED_BIT);
}
