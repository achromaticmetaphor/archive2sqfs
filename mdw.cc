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

#include <cstddef>
#include <cstdint>

#include <zlib.h>

#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"

template <typename T>
static inline auto rup(T const a, T const s)
{
  auto mask = ~(SIZE_MAX << s);
  auto factor = (a >> s) + ((a & mask) != 0);
  return (std::size_t(1) << s) * factor;
}

void mdw::write_block_compressed(std::size_t const block_len, unsigned char const * const block, uint16_t const bsize)
{
  uint8_t tmp[2];
  le16(tmp, bsize);
  table.push_back(tmp[0]);
  table.push_back(tmp[1]);

  for (std::size_t i = 0; i < block_len; ++i)
    table.push_back(block[i]);
}

void mdw::write_block_no_pad(void)
{
  if (buff.size() == 0)
    return;

  unsigned long int zsize = compressBound(buff.size());
  unsigned char zbuff[zsize];
  compress2(zbuff, &zsize, buff.data(), buff.size(), 9);

  bool const compressed = zsize < buff.size();
  auto buf = compressed ? zbuff : buff.data();
  auto const size = compressed ? zsize : buff.size();

  write_block_compressed(size, buf, compressed ? size : (size | SQFS_META_BLOCK_COMPRESSED_BIT));
  buff.clear();
}

void mdw::write_block(void)
{
  while (buff.size() < SQFS_META_BLOCK_SIZE)
    buff.push_back(0);
  write_block_no_pad();
}

uint64_t mdw::put(unsigned char const * b, std::size_t len)
{
  uint64_t const addr = meta_address(table.size(), buff.size());

  while (len != 0)
    {
      auto const remaining = SQFS_META_BLOCK_SIZE - buff.size();
      auto const added = len > remaining ? remaining : len;

      for (std::size_t i = 0; i < added; ++i)
        buff.push_back(b[i]);
      if (buff.size() == SQFS_META_BLOCK_SIZE)
        write_block();

      len -= added;
      b += added;
    }

  return addr;
}
