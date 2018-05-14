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

using namespace std::literals;

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

  std::future<compression_result>
  compress_async(block_type && in,
                 std::launch const policy = std::launch::async)
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

#if LSL_ENABLE_COMP_zstd
struct compressor_zstd : public compressor
{
  virtual compression_result compress(block_type &&);
  compressor_zstd() : compressor(SQFS_COMPRESSION_TYPE_ZSTD) {}
};
#endif

struct compressor_none : public compressor
{
  virtual compression_result compress(block_type && in)
  {
    return {std::move(in), false};
  }
  compressor_none() : compressor(SQFS_COMPRESSION_TYPE_ZLIB) {}
};

static inline compressor * get_compressor_for(std::string const & type)
{
#define RETURN_IF(N)                                                         \
  if (type == #N)                                                            \
    return new compressor_##N {}
  RETURN_IF(zlib);
#if LSL_ENABLE_COMP_zstd
  RETURN_IF(zstd);
#endif
  RETURN_IF(none);
  throw std::runtime_error("unknown compression type: "s + type);
#undef RETURN_IF
}

static std::string const COMPRESSOR_DEFAULT = "zlib";

template <typename C>
static bool compress_data(C comp, std::vector<unsigned char> & out,
                          std::vector<unsigned char> const && in)
{
  if (in.empty())
    return false;

  comp(out, in);

  bool const compressed = out.size() < in.size();
  if (!compressed)
    out = std::move(in);

  return compressed;
}
#endif
