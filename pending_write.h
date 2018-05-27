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
#include <future>

#include "compressor.h"

struct sqsh_writer;

struct pending_write
{
  sqsh_writer & writer;
  std::future<compression_result> future;

  pending_write(sqsh_writer & writer,
                std::future<compression_result> && future)
      : writer(writer), future(std::move(future))
  {
  }

  virtual ~pending_write() = default;

  virtual void report(uint64_t, std::vector<char> &, bool) = 0;
  virtual void handle_write();
};

struct pending_fragment : public pending_write
{
  pending_fragment(sqsh_writer & writer,
                   std::future<compression_result> && future)
      : pending_write(writer, std::move(future))
  {
  }

  virtual void report(uint64_t, std::vector<char> &, bool);
};

struct pending_block : public pending_write
{
  uint32_t inode_number;

  pending_block(sqsh_writer & writer,
                std::future<compression_result> && future,
                uint32_t inode_number)
      : pending_write(writer, std::move(future)), inode_number(inode_number)
  {
  }

  virtual void report(uint64_t, std::vector<char> &, bool);
};

struct pending_dedup : public pending_write
{
  uint32_t const inode_number;

  pending_dedup(sqsh_writer & writer, uint32_t inode_number)
      : pending_write(writer, {}), inode_number(inode_number)
  {
  }

  virtual void report(uint64_t, std::vector<char> &, bool) {}
  virtual void handle_write();
};

#endif
