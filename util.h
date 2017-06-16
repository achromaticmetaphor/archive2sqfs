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

#ifndef LSL_UTIL_H
#define LSL_UTIL_H

#define RETIF_C(C, CL)       \
  do                         \
    {                        \
      int const error = (C); \
      if (error)             \
        {                    \
          CL;                \
          return error;      \
        }                    \
    }                        \
  while (0)

#define RETIF(C) RETIF_C((C), )

#define MASK_LOW(N) (~((~0u) << (N)))

#endif
