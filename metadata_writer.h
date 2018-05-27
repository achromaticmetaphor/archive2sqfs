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

#ifndef LSL_METADATA_WRITER_H
#define LSL_METADATA_WRITER_H

#include <ostream>
#include <vector>

#include "compressor.h"
#include "sqsh_defs.h"

struct metadata_writer
{
  compressor & comp;
  std::vector<char> table;
  std::vector<char> buff;

  void out(std::ostream & out) { out.write(table.data(), table.size()); }

  meta_address get_address()
  {
    return meta_address(table.size(), buff.size());
  }

  void write_block_compressed(std::vector<char> const &, uint16_t);
  void write_block_no_pad(void);
  void write_block(void);
  meta_address put(char const *, size_t);

  template <typename C> meta_address put(C const & c)
  {
    return put(c.data(), c.size());
  }

  metadata_writer(compressor & comp) : comp(comp) {}
};

#endif
