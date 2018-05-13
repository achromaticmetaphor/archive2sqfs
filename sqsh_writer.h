/*
Copyright (C) 2016, 2017, 2018  Charles Cagle

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

#include <atomic>
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

#define SQFS_BLOCK_LOG_DEFAULT 17

struct sqfs_super
{
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

  std::ofstream outfile;

  std::vector<unsigned char> current_block;
  std::vector<unsigned char> current_fragment;
  std::vector<fragment_entry> fragments;
  uint32_t fragment_count = 0;
  std::unordered_map<uint32_t, uint16_t> ids;
  std::unordered_map<uint16_t, uint32_t> rids;
  std::unordered_map<uint32_t, block_report> reports;

  std::thread thread;
  bool single_threaded;
  bounded_work_queue<std::unique_ptr<pending_write>> writer_queue;
  std::unique_ptr<compressor> comp;
  std::atomic<bool> writer_failed{false};

  struct mdw dentry_writer;
  struct mdw inode_writer;

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

  uint32_t next_inode_number() { return next_inode++; }

  void write_header();
  size_t put_fragment();
  void flush_fragment();
  void write_tables();
  void enqueue_block(uint32_t);
  void enqueue_fragment();
  void writer_thread();
  bool finish_data();

  template <typename P>
  sqsh_writer(P path, int blog, std::string comptype,
              bool disable_threads = false)
      : outfile(path, std::ios_base::binary),
        single_threaded(disable_threads), writer_queue(thread_count()),
        comp(get_compressor_for(comptype)), dentry_writer(*comp),
        inode_writer(*comp)
  {
    super.block_log = blog;
    outfile.exceptions(std::ios_base::failbit);
    outfile.seekp(SQFS_SUPER_SIZE);
    if (!single_threaded)
      thread = std::thread(&sqsh_writer::writer_thread, this);
  }

  ~sqsh_writer()
  {
    if (thread.joinable())
      finish_data();
  }
};

#endif
