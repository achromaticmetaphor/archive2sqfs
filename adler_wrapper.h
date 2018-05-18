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

#ifndef LSL_ADLER_WRAPPER_H
#define LSL_ADLER_WRAPPER_H

#include <zlib.h>

struct adler_wrapper
{
  uLong sum;

  adler_wrapper() : sum(adler32_z(0, Z_NULL, 0)) {}

  template <typename C> void update(C & c)
  {
    sum = adler32_z(sum, reinterpret_cast<Bytef *>(c.data()), c.size());
  }
};

#endif
