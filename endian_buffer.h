/*
Copyright (C) 2018 Charles Cagle

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

#ifndef LSL_ENDIAN_BUFFER_H
#define LSL_ENDIAN_BUFFER_H

#include <array>
#include <cstdint>
#include <vector>

class endian_buffer_base
{
public:
  virtual void l8(uint8_t) = 0;
  virtual void l8(std::size_t, uint8_t) = 0;

  void l16(uint16_t const n)
  {
    l8(n & 0xff);
    l8((n >> 8) & 0xff);
  }

  void l16(std::size_t const offset, uint16_t const n)
  {
    l8(offset, n & 0xff);
    l8(offset + 1, (n >> 8) & 0xff);
  }

  void l32(uint32_t const n)
  {
    l16(n & 0xffffu);
    l16((n >> 16) & 0xffffu);
  }

  void l64(uint64_t const n)
  {
    l32(n & 0xffffffffu);
    l32((n >> 32) & 0xffffffffu);
  }
};

template <std::size_t N> class endian_buffer : public endian_buffer_base
{
  std::array<char, N> arr;
  std::size_t index = 0;

public:
  virtual void l8(std::size_t i, uint8_t const n) { arr[i] = n; }

  virtual void l8(uint8_t const n) { l8(index++, n); }

  char const * data() const { return arr.data(); }
  std::size_t size() const { return index; }
  auto operator[](std::size_t n) const { return arr[n]; }
};

template <> class endian_buffer<0> : public endian_buffer_base
{
  std::vector<char> vec;

public:
  virtual void l8(uint8_t const n) { vec.push_back(n); }

  virtual void l8(std::size_t i, uint8_t const n) { vec[i] = n; }

  char const * data() const { return vec.data(); }
  std::size_t size() const { return vec.size(); }
  auto operator[](std::size_t n) const { return vec[n]; }
};

#endif
