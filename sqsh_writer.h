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

#include <stdint.h>
#include <stdio.h>

#include <unordered_map>
#include <vector>

#include <search.h>

#include "mdw.h"

#define SQFS_BLOCK_LOG_DEFAULT 17

struct fragment_entry
{
  uint64_t start_block;
  uint32_t size;
};

struct sqfs_super
{
  uint16_t compression;
  uint16_t block_log;
  uint16_t flags;

  uint64_t root_inode;
  uint64_t bytes_used;
  uint64_t id_table_start;
  uint64_t xattr_table_start;
  uint64_t inode_table_start;
  uint64_t directory_table_start;
  uint64_t fragment_table_start;
  uint64_t lookup_table_start;
};

struct sqsh_writer
{
  uint32_t next_inode;
  struct sqfs_super super;
  struct mdw dentry_writer;
  struct mdw inode_writer;
  FILE * outfile;
  unsigned char * current_block;
  size_t current_pos;
  unsigned char * current_fragment;
  size_t fragment_pos;
  std::vector<fragment_entry> fragments;
  std::unordered_map<uint32_t, uint16_t> ids;
  std::unordered_map<uint16_t, uint32_t> rids;

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
};

int u32cmp(void const *, void const *);
int sqsh_writer_init(struct sqsh_writer *, char const *, int);
int sqsh_writer_destroy(struct sqsh_writer *);
int sqsh_writer_write_header(struct sqsh_writer *);
size_t sqsh_writer_put_fragment(struct sqsh_writer *, unsigned char const *, size_t);
int sqsh_writer_flush_fragment(struct sqsh_writer *);
int sqsh_writer_write_tables(struct sqsh_writer *);

static inline uint32_t sqsh_writer_next_inode_number(struct sqsh_writer * const wr)
{
  return wr->next_inode++;
}

#endif
