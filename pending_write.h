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

#include "block_report.h"
#include "compressor.h"
#include "fragment_entry.h"

template <typename T> struct pending_write
{
  T & writer;
  std::future<compression_result> future;

  pending_write(T & writer, std::future<compression_result> && future)
      : writer(writer), future(std::move(future))
  {
  }
  virtual ~pending_write() = default;

  virtual void report(uint64_t, std::vector<char> &, bool) = 0;

  virtual void handle_write()
  {
    auto result = future.get();
    report(writer.write_bytes(result.block), result.block, result.compressed);
  }
};

template <typename T> struct pending_fragment : public pending_write<T>
{
  pending_fragment(T & writer, std::future<compression_result> && future)
      : pending_write<T>(writer, std::move(future))
  {
  }

  virtual void report(uint64_t start, std::vector<char> & block,
                      bool compressed)
  {
    uint32_t const size = block.size();
    pending_write<T>::writer.push_fragment_entry(
        {start, size | (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT)});
  }
};

template <typename T> struct pending_block : public pending_write<T>
{
  uint32_t inode_number;

  pending_block(T & writer, std::future<compression_result> && future,
                uint32_t inode_number)
      : pending_write<T>(writer, std::move(future)),
        inode_number(inode_number)
  {
  }

  virtual void report(uint64_t start, std::vector<char> & block,
                      bool compressed)
  {
    auto & report = pending_write<T>::writer.reports[inode_number];
    if (report.sizes.empty())
      report.start_block = start;
    report.sizes.push_back(block.size() |
                           (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT));
  }
};

#endif
