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

#ifndef LSL_COMPRESSOR_H
#define LSL_COMPRESSOR_H

#include <future>
#include <memory>
#include <utility>
#include <vector>

using block_type = std::unique_ptr<std::vector<unsigned char>>;

static inline block_type get_block()
{
  return std::make_unique<block_type::element_type>();
}

struct compression_result
{
  uint32_t msize;
  block_type block;
};

struct compressor
{
  virtual compression_result compress(block_type &&, block_type &&) = 0;
  virtual ~compressor() = default;

  std::future<compression_result> compress_async(block_type && in)
  {
    return std::async(std::launch::async, &compressor::compress, this, get_block(), std::move(in));
  }
};

struct compressor_zlib : public compressor
{
  virtual compression_result compress(block_type &&, block_type &&);
};

#endif
