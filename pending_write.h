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

#ifndef LSL_PENDING_WRITE_H
#define LSL_PENDING_WRITE_H

#include <cstdint>
#include <fstream>
#include <future>
#include <memory>
#include <utility>
#include <vector>

#include "compressor.h"
#include "fragment_entry.h"
#include "util.h"

struct pending_write
{
  std::ofstream & out;
  std::future<compression_result> future;

  pending_write(std::ofstream & out, std::future<compression_result> && future) : out(out), future(std::move(future)) {}
  virtual ~pending_write() = default;

  virtual void report(uint64_t, uint32_t) = 0;

  int handle_write()
  {
    auto const tell = out.tellp();
    RETIF(tell == -1);

    auto result = future.get();
    RETIF(result.msize == SQFS_BLOCK_INVALID);

    for (auto const & c : *result.block)
      out << c;
    RETIF(out.fail());

    report(tell, result.msize);
    return 0;
  }
};

struct pending_fragment : public pending_write
{
  std::vector<fragment_entry> & fragments;

  pending_fragment(std::ofstream & out, std::future<compression_result> && future, std::vector<fragment_entry> & frags) : pending_write(out, std::move(future)), fragments(frags) {}

  virtual void report(uint64_t start, uint32_t size)
  {
    fragments.push_back({start, size});
  }
};

struct pending_block : public pending_write
{
  std::shared_ptr<std::vector<uint32_t>> sizes;
  std::shared_ptr<uint64_t> start_block;

  pending_block(std::ofstream & out, std::future<compression_result> && future, std::shared_ptr<std::vector<uint32_t>> sizes, std::shared_ptr<uint64_t> start) : pending_write(out, std::move(future)), sizes(sizes), start_block(start) {}

  virtual void report(uint64_t start, uint32_t size)
  {
    if (sizes->empty())
      *start_block = start;
    sizes->push_back(size);
  }
};

#endif
