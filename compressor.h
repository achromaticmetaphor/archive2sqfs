/*
Copyright (C) 2016, 2017, 2018  Charles Cagle

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
#include <string>
#include <utility>
#include <vector>

#include "sqsh_defs.h"

using block_type = std::vector<unsigned char>;

struct compression_result
{
  block_type block;
  bool compressed;
};

struct compressor
{
  uint16_t const type;
  virtual compression_result compress(block_type &&) = 0;
  virtual ~compressor() = default;

  std::future<compression_result> compress_async(block_type && in, std::launch const policy = std::launch::async)
  {
    return std::async(policy, &compressor::compress, this, std::move(in));
  }

  compressor(uint16_t type) : type(type) {}
};

struct compressor_zlib : public compressor
{
  virtual compression_result compress(block_type &&);
  compressor_zlib() : compressor(SQFS_COMPRESSION_TYPE_ZLIB) {}
};

struct compressor_none : public compressor
{
  virtual compression_result compress(block_type && in) { return {std::move(in), false}; }
  compressor_none() : compressor(SQFS_COMPRESSION_TYPE_ZLIB) {}
};

#include <iostream>

static inline compressor * get_compressor_for(std::string const & type)
{
  if (type == "zlib")
    return new compressor_zlib{};
  else if (type == "none")
    return new compressor_none{};
  else
    return new compressor_zlib{};
}

static std::string const COMPRESSOR_DEFAULT = "zlib";

#endif
