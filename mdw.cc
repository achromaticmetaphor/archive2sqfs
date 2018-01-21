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
#include <vector>

#include "endian_buffer.h"
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
  endian_buffer<2> buff;
  buff.l16(bsize);
  table.push_back(buff[0]);
  table.push_back(buff[1]);

  for (std::size_t i = 0; i < block_len; ++i)
    table.push_back(block[i]);
}

bool mdw::write_block_no_pad(void)
{
  if (buff.empty())
    return false;

  auto comp_opt = comp.compress(block_type{buff});
  if (!comp_opt)
    return true;

  auto const size = comp_opt->block.size();
  write_block_compressed(size, comp_opt->block.data(), comp_opt->compressed ? size : (size | SQFS_META_BLOCK_COMPRESSED_BIT));
  buff.clear();

  return false;
}

bool mdw::write_block(void)
{
  while (buff.size() < SQFS_META_BLOCK_SIZE)
    buff.push_back(0);
  return write_block_no_pad();
}

meta_address mdw::put(unsigned char const * b, std::size_t len)
{
  meta_address const addr = get_address();

  while (len != 0)
    {
      auto const remaining = SQFS_META_BLOCK_SIZE - buff.size();
      auto const added = len > remaining ? remaining : len;

      for (std::size_t i = 0; i < added; ++i)
        buff.push_back(b[i]);
      if (buff.size() == SQFS_META_BLOCK_SIZE)
        if (write_block())
          return 1;

      len -= added;
      b += added;
    }

  return addr;
}
