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

#include <iostream>

#include "compressor.h"
#include "optional.h"
#include "sqsh_defs.h"

static bool compress_zlib(std::vector<unsigned char> & out, std::vector<unsigned char> const & in)
{
  auto zsize = compressBound(in.size());
  out.resize(zsize);
  if (compress2(out.data(), &zsize, in.data(), in.size(), 9) != Z_OK)
    return false;
  out.resize(zsize);
  return true;
}

template <typename C>
static optional<bool> compress_data(C comp, std::vector<unsigned char> & out, std::vector<unsigned char> const && in)
{
  if (in.empty())
    return false;

  if (!comp(out, in))
    return {};

  bool const compressed = out.size() < in.size();
  if (!compressed)
    out = std::move(in);

  return bool(compressed);
}

optional<compression_result> compressor_zlib::compress(block_type && in)
{
  block_type out;
  auto const compressed_opt = compress_data(compress_zlib, out, std::move(in));
  if (compressed_opt.has_value)
    return {{std::move(out), compressed_opt.value}};
  else
    return {};
}
