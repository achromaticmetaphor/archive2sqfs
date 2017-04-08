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

#ifndef LSL_DIRTREE_H
#define LSL_DIRTREE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "sqsh_writer.h"

struct dirtree
{
  uint16_t inode_type;
  uint16_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t mtime;
  uint32_t inode_number;
  uint64_t inode_address;
  uint32_t nlink;
  uint32_t xattr;

  virtual ~dirtree() = default;
};

struct dirtree_entry
{
  char const * name;
  std::shared_ptr<dirtree> inode;
};

struct dirtree_dir : public dirtree
{
  size_t space;
  std::vector<dirtree_entry> * entries;
  uint32_t filesize;
  uint32_t dtable_start_block;
  uint16_t dtable_start_offset;
};

struct dirtree_reg : public dirtree
{
  uint64_t start_block;
  uint64_t file_size;
  uint64_t sparse;
  uint32_t fragment;
  uint32_t offset;

  std::vector<uint32_t> * blocks;
};

struct dirtree_sym : public dirtree
{
  char * target;
};

struct dirtree_dev : public dirtree
{
  uint32_t rdev;
};

int dirtree_entry_compare(void const *, void const *);
int dirtree_entry_by_name(void const *, void const *);
void dirtree_reg_init(dirtree &, struct sqsh_writer *);
void dirtree_dir_init(dirtree &, struct sqsh_writer *);
std::shared_ptr<dirtree> dirtree_reg_new(struct sqsh_writer *);
std::shared_ptr<dirtree> dirtree_dir_new(struct sqsh_writer *);
std::shared_ptr<dirtree> dirtree_get_subdir_for_path(struct sqsh_writer *, std::shared_ptr<dirtree>, char const *);
void dirtree_dump_tree(struct dirtree const *);
int dirtree_write_tables(struct sqsh_writer *, struct dirtree *);
void dirtree_free(std::shared_ptr<dirtree>);
std::shared_ptr<dirtree> dirtree_put_reg_for_path(struct sqsh_writer *, std::shared_ptr<dirtree>, char const *);
int dirtree_reg_append(struct sqsh_writer *, struct dirtree *, unsigned char const *, size_t);
int dirtree_reg_flush(struct sqsh_writer *, struct dirtree *);
std::shared_ptr<dirtree> dirtree_put_sym_for_path(struct sqsh_writer *, std::shared_ptr<dirtree>, char const *, char const *);
std::shared_ptr<dirtree> dirtree_put_dev_for_path(struct sqsh_writer *, std::shared_ptr<dirtree>, char const *, uint16_t, uint32_t);
std::shared_ptr<dirtree> dirtree_put_ipc_for_path(struct sqsh_writer *, std::shared_ptr<dirtree>, char const *, uint16_t);
std::shared_ptr<dirtree> dirtree_sym_new(struct sqsh_writer *);
std::shared_ptr<dirtree> dirtree_dev_new(struct sqsh_writer *);
std::shared_ptr<dirtree> dirtree_ipc_new(struct sqsh_writer *);

static inline void dirtree_init(dirtree & dt, struct sqsh_writer * const wr)
{
  dt.mode = 0644;
  dt.uid = 0;
  dt.gid = 0;
  dt.mtime = 0;
  dt.inode_number = sqsh_writer_next_inode_number(wr);
  dt.nlink = 1;
  dt.xattr = 0xffffffffu;
}

#endif
