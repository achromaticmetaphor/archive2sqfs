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

#include <utility>
#include <vector>

#include <zlib.h>

#include "compressor.h"
#include "sqsh_defs.h"

static uint32_t compress_data_zlib(std::vector<unsigned char> & out, std::vector<unsigned char> const && in)
{
  if (in.size() == 0)
    return SQFS_BLOCK_COMPRESSED_BIT;

  auto zsize = compressBound(in.size());
  out.resize(zsize);
  if (compress2(out.data(), &zsize, in.data(), in.size(), 9) != Z_OK)
    return SQFS_BLOCK_INVALID;
  out.resize(zsize);

  bool const compressed = out.size() < in.size();
  if (!compressed)
    out = std::move(in);

  return compressed ? out.size() : (out.size() | SQFS_BLOCK_COMPRESSED_BIT);
}

compression_result compressor_zlib::compress(block_type && out, block_type && in)
{
  auto const bsize = compress_data_zlib(*out, std::move(*in));
  return {bsize, std::move(out)};
}
