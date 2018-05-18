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

#ifndef LSL_FSTREAM_UTIL_H
#define LSL_FSTREAM_UTIL_H

#include <fstream>

struct restore_pos
{
  std::fstream & fs;
  std::fstream::pos_type const gpos;
  std::fstream::pos_type const ppos;

  restore_pos(std::fstream & fs) : fs(fs), gpos(fs.tellg()), ppos(fs.tellp())
  {
  }

  ~restore_pos()
  {
    fs.seekg(gpos);
    fs.seekp(ppos);
  }
};

template <typename T>
static bool range_equal(T & reader, std::fstream::pos_type i1,
                        std::fstream::pos_type i2, std::streamsize len,
                        std::streamsize const block_size = 4096)
{
  while (len > block_size)
    {
      if (reader.read_bytes(i1, block_size) !=
          reader.read_bytes(i2, block_size))
        return false;
      len -= block_size;
      i1 += block_size;
      i2 += block_size;
    }
  return reader.read_bytes(i1, len) == reader.read_bytes(i2, len);
}

#endif
