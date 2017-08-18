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

#ifndef LSL_SQSH_WRITER_H
#define LSL_SQSH_WRITER_H

#include <cstdint>
#include <fstream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bounded_work_queue.h"
#include "compressor.h"
#include "fragment_entry.h"
#include "mdw.h"
#include "pending_write.h"
#include "sqsh_defs.h"
#include "util.h"

#define SQFS_BLOCK_LOG_DEFAULT 17
#define SQFS_COMPRESSION_TYPE_DEFAULT SQFS_COMPRESSION_TYPE_ZLIB

struct sqfs_super
{
  uint16_t compression = SQFS_COMPRESSION_TYPE_DEFAULT;
  uint16_t block_log = SQFS_BLOCK_LOG_DEFAULT;
  uint16_t flags = 0;

  meta_address root_inode;
  uint64_t bytes_used = 0;
  uint64_t id_table_start = 0;
  uint64_t xattr_table_start = SQFS_TABLE_NOT_PRESENT;
  uint64_t inode_table_start = 0;
  uint64_t directory_table_start = 0;
  uint64_t fragment_table_start = 0;
  uint64_t lookup_table_start = SQFS_TABLE_NOT_PRESENT;
};

static inline auto thread_count()
{
  auto tc = std::thread::hardware_concurrency();
  return 2 + (tc > 0 ? tc : 4);
}

struct sqsh_writer
{
  uint32_t next_inode = 1;
  struct sqfs_super super;

  struct mdw dentry_writer;
  struct mdw inode_writer;
  std::ofstream outfile;

  std::unique_ptr<std::vector<unsigned char>> current_block;
  std::unique_ptr<std::vector<unsigned char>> current_fragment;
  std::vector<fragment_entry> fragments;
  uint32_t fragment_count = 0;
  std::unordered_map<uint32_t, uint16_t> ids;
  std::unordered_map<uint16_t, uint32_t> rids;

  std::thread thread;
  bounded_work_queue<std::unique_ptr<pending_write>> writer_queue;
  std::shared_ptr<compressor> comp;
  bool writer_failed = false;

  uint16_t id_lookup(uint32_t const id)
  {
    auto found = ids.find(id);
    if (found == ids.end())
      {
        auto next = ids.size();
        ids[id] = next;
        rids[next] = id;
        return next;
      }
    else
      return found->second;
  }

  uint32_t next_inode_number()
  {
    return next_inode++;
  }

  int write_header();
  size_t put_fragment();
  void flush_fragment();
  int write_tables();
  void enqueue_block(std::shared_ptr<std::vector<uint32_t>>, std::shared_ptr<uint64_t>);
  void enqueue_fragment();
  void writer_thread();
  bool finish_data();

  sqsh_writer(char const * path, int blog = SQFS_BLOCK_LOG_DEFAULT) : outfile(path, std::ios_base::binary), writer_queue(thread_count()), comp(new compressor_zlib())
  {
    super.block_log = blog;
    outfile.seekp(SQFS_SUPER_SIZE);
    current_block = get_block();
    current_fragment = get_block();
    thread = std::thread(&sqsh_writer::writer_thread, this);
  }

  ~sqsh_writer()
  {
    if (thread.joinable())
      finish_data();
  }
};

#endif
