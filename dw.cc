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

#include <ostream>
#include <vector>

#include <zlib.h>

#include "dw.h"
#include "sqsh_defs.h"

uint32_t dw_write_data(std::vector<unsigned char> const & buff, std::ostream & out)
{
  if (buff.size() == 0)
    return SQFS_BLOCK_COMPRESSED_BIT;

  auto zsize = compressBound(buff.size());
  unsigned char zbuff[zsize];
  if (compress2(zbuff, &zsize, buff.data(), buff.size(), 9) != Z_OK)
    return 0xffffffff;

  bool const compressed = zsize < buff.size();
  auto const block = compressed ? zbuff : buff.data();
  auto const size = compressed ? zsize : buff.size();

  for (auto i = 0; i < size; ++i)
    out << block[i];
  if (out.fail())
    return 0xffffffff;

  return compressed ? size : (size | SQFS_BLOCK_COMPRESSED_BIT);
}
