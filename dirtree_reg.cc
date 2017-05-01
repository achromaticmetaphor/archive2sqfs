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

#include <cstddef>
#include <cstdint>

#include "dirtree.h"
#include "dw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"
#include "util.h"

void dirtree_reg::add_block(std::size_t size, long int const sb)
{
  if (blocks.size() == 0)
    start_block = sb;
  blocks.push_back(size);
}

int dirtree_reg::flush()
{
  if (wr->current_block.size() == 0)
    return 0;

  auto const block_size = std::size_t(1) << wr->super.block_log;
  if (wr->current_block.size() < block_size && blocks.size() == 0)
    {
      offset = wr->put_fragment();
      RETIF(offset == block_size);
      fragment = wr->fragments.size();
    }
  else
    {
      auto const tell = wr->outfile.tellp();
      RETIF(tell == -1);

      uint32_t const bsize = dw_write_data(wr->current_block, wr->outfile);
      RETIF(bsize == 0xffffffff);

      add_block(bsize, tell);
    }

  wr->current_block.clear();
  return 0;
}

int dirtree_reg::append(unsigned char const * buff, std::size_t len)
{
  file_size += len;
  auto const block_size = std::size_t(1) << wr->super.block_log;
  while (len != 0)
    {
      auto const remaining = block_size - wr->current_block.size();
      auto const added = len > remaining ? remaining : len;
      for (std::size_t i = 0; i < added; ++i)
        wr->current_block.push_back(buff[i]);

      if (wr->current_block.size() == block_size)
        RETIF(flush());

      len -= added;
      buff += added;
    }

  return 0;
}
