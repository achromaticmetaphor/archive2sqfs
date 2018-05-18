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

#ifndef LSL_FILESYSTEM_H
#define LSL_FILESYSTEM_H

#if __cplusplus >= 201703L && __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::filesystem;

#elif LSL_USE_POSIX
#include <stdexcept>
#include <unistd.h>

namespace filesystem
{
inline void resize_file(std::string const & path, std::uintmax_t len)
{
  if (truncate(path.data(), len) != 0)
    throw std::runtime_error{"POSIX truncate() failed"s};
}
}

#elif LSL_USE_BOOST
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;

#else
#error archive2sqfs requires one of C++17, POSIX:2008, or Boost-1.30.0. \
  C++17 is used automatically if available. \
  Use cmake -DUSE_POSIX=1 or -DUSE_BOOST=1 otherwise.

#endif

#endif
