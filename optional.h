/*
Copyright (C) 2017  Charles Cagle

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

#ifndef LSL_OPTIONAL_H
#define LSL_OPTIONAL_H

template <typename T>
struct optional
{
  bool has_value;
  T value;

  optional() : has_value(false) {}
  optional(T && value) : has_value(true), value(std::move(value)) {}
};

#endif
