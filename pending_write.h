/*
Copyright (C) 2017, 2018  Charles Cagle

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
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::literals;

#include "compressor.h"
#include "fragment_entry.h"

struct block_report
{
  uint64_t start_block;
  std::vector<uint32_t> sizes;
};

struct pending_write
{
  std::ofstream & out;
  std::future<compression_result> future;

  pending_write(std::ofstream & out, std::future<compression_result> && future) : out(out), future(std::move(future)) {}
  virtual ~pending_write() = default;

  virtual void report(uint64_t, uint32_t, bool) = 0;

  void handle_write()
  {
    auto const tell = out.tellp();
    auto result = future.get();
    out.write(reinterpret_cast<char *>(result.block.data()), result.block.size());
    report(tell, result.block.size(), result.compressed);
  }
};

struct pending_fragment : public pending_write
{
  std::vector<fragment_entry> & fragments;

  pending_fragment(std::ofstream & out, std::future<compression_result> && future, std::vector<fragment_entry> & frags) : pending_write(out, std::move(future)), fragments(frags) {}

  virtual void report(uint64_t start, uint32_t size, bool compressed)
  {
    fragments.push_back({start, size | (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT)});
  }
};

struct pending_block : public pending_write
{
  uint32_t inode_number;
  std::unordered_map<uint32_t, block_report> & reports;

  pending_block(std::ofstream & out, std::future<compression_result> && future, uint32_t inode_number, std::unordered_map<uint32_t, block_report> & reports) : pending_write(out, std::move(future)), inode_number(inode_number), reports(reports) {}

  virtual void report(uint64_t start, uint32_t size, bool compressed)
  {
    auto & report = reports[inode_number];
    if (report.sizes.empty())
      report.start_block = start;
    report.sizes.push_back(size | (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT));
  }
};

#endif
