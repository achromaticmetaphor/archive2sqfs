/*
Copyright (C) 2018  Charles Cagle

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

#ifndef LSL_BLOCK_REPORT_H
#define LSL_BLOCK_REPORT_H

#include <cstddef>
#include <fstream>
#include <vector>

#include "fstream_util.h"
#include "sqsh_defs.h"

struct block_report
{
  uint64_t start_block;
  std::vector<uint32_t> sizes;

  uint64_t range_len() const
  {
    uint64_t sum = 0;
    for (auto s : sizes)
      sum += s & ~SQFS_BLOCK_COMPRESSED_BIT;
    return sum;
  }

  bool same_shape(block_report const & br) const { return sizes == br.sizes; }

  template <typename T>
  bool same_content(block_report const & br, T & reader) const
  {
    return same_shape(br) &&
           range_equal(reader, start_block, br.start_block, range_len());
  }
};

#endif
