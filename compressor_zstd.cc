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

#if LSL_ENABLE_COMP_zstd

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

#include <zstd.h>

#include "compressor.h"
#include "sqsh_defs.h"

static void compress_zstd(block_type & out, block_type const & in)
{
  out.resize(ZSTD_compressBound(in.size()));
  auto const result =
      ZSTD_compress(out.data(), out.size(), in.data(), in.size(), 15);
  if (ZSTD_isError(result))
    throw std::runtime_error("failure in ZSTD_compress"s);
  out.resize(result);
}

compression_result compressor_zstd::compress(block_type && in)
{
  block_type out;
  bool const compressed = compress_data(compress_zstd, out, std::move(in));
  return {std::move(out), compressed};
}

block_type compressor_zstd::decompress(block_type && in,
                                       std::size_t const bound)
{
  block_type out;
  out.resize(bound);
  auto const result =
      ZSTD_decompress(out.data(), out.size(), in.data(), in.size());
  if (ZSTD_isError(result))
    throw std::runtime_error("failure in ZSTD_decompress"s);
  out.resize(result);
  return out;
}

#endif
