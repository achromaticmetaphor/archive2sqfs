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

#ifndef LSL_LE_H
#define LSL_LE_H

#include <cstdint>

static inline void le16(uint8_t * const out, uint16_t const n)
{
  out[0] = n & 0xff;
  out[1] = (n >> 8) & 0xff;
}

static inline void le32(uint8_t * const out, uint32_t const n)
{
  le16(out, n & 0xffffu);
  le16(out + 2, (n >> 16) & 0xffffu);
}

static inline void le64(uint8_t * const out, uint64_t const n)
{
  le32(out, n & 0xffffffffu);
  le32(out + 4, (n >> 32) & 0xffffffffu);
}

static inline uint16_t le16_r(uint8_t const * const in)
{
  return in[0] | (in[1] << 8);
}

static inline uint32_t le32_r(uint8_t const * const in)
{
  return le16_r(in) | ((uint32_t) le16_r(in + 2) << 16);
}

static inline uint64_t le64_r(uint8_t const * const in)
{
  return le32_r(in) | ((uint64_t) le32_r(in + 4) << 32);
}

#endif
