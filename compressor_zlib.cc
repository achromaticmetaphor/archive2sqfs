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

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

#include <zlib.h>

#include "compressor.h"
#include "sqsh_defs.h"

static void compress_zlib(std::vector<unsigned char> & out,
                          std::vector<unsigned char> const & in)
{
  auto zsize = compressBound(in.size());
  out.resize(zsize);
  if (compress2(out.data(), &zsize, in.data(), in.size(), 9) != Z_OK)
    throw std::runtime_error("failure in zlib::compress2"s);
  out.resize(zsize);
}

compression_result compressor_zlib::compress(block_type && in)
{
  block_type out;
  bool const compressed = compress_data(compress_zlib, out, std::move(in));
  return {std::move(out), compressed};
}
