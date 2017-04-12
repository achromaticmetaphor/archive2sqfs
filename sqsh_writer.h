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
  uint16_t compression = 1;
  uint16_t block_log = SQFS_BLOCK_LOG_DEFAULT;
  uint16_t flags = 0;

  uint64_t root_inode = 0;
  uint64_t bytes_used = 0;
  uint64_t id_table_start = 0;
  uint64_t xattr_table_start = 0xffffffffffffffffu;
  uint64_t inode_table_start = 0;
  uint64_t directory_table_start = 0;
  uint64_t fragment_table_start = 0;
  uint64_t lookup_table_start = 0xffffffffffffffffu;
};

struct sqsh_writer
{
  uint32_t next_inode = 1;
  struct sqfs_super super;
  struct mdw dentry_writer;
  struct mdw inode_writer;
  FILE * outfile;
  std::vector<unsigned char> current_block;
  std::vector<unsigned char> current_fragment;
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

  uint32_t next_inode_number()
  {
    return next_inode++;
  }

  int write_header();
  size_t put_fragment();
  int flush_fragment();
  int write_tables();

  sqsh_writer(char const * path, int blog = SQFS_BLOCK_LOG_DEFAULT)
  {
    super.block_log = blog;
    outfile = fopen(path, "wb");
    if (outfile != nullptr)
      fseek(outfile, 96L, SEEK_SET);
  }

  ~sqsh_writer()
  {
    if (outfile != nullptr)
      fclose(outfile);
  }
};

#endif
