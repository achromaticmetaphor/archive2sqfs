/*
Copyright (C) 2016  Charles Cagle

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

#include "sqsh_writer.h"

struct dirtree_entry
{
  char const * name;
  struct dirtree * inode;
};

struct dirtree_addi_dir
{
  size_t nentries;
  size_t space;
  struct dirtree_entry * entries;
  uint32_t filesize;
  uint32_t nlink;
  uint32_t dtable_start_block;
  uint16_t dtable_start_offset;
};

struct dirtree_addi_reg
{
  uint64_t start_block;
  uint64_t file_size;
  uint64_t sparse;
  uint32_t nlink;
  uint32_t fragment;
  uint32_t offset;
  uint32_t xattr;

  uint32_t * blocks;
  size_t nblocks;
  size_t blocks_space;
};

union dirtree_addi
{
  struct dirtree_addi_dir dir;
  struct dirtree_addi_reg reg;
};

struct dirtree
{
  uint16_t inode_type;
  uint16_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t mtime;
  uint32_t inode_number;
  uint64_t inode_address;

  union dirtree_addi addi;
};

int dirtree_entry_compare(void const *, void const *);
int dirtree_entry_by_name(void const *, void const *);
void dirtree_reg_init(struct dirtree *, struct sqsh_writer *);
void dirtree_dir_init(struct dirtree *, struct sqsh_writer *);
struct dirtree * dirtree_reg_new(struct sqsh_writer *);
struct dirtree * dirtree_dir_new(struct sqsh_writer *);
struct dirtree * dirtree_get_subdir_for_path(struct sqsh_writer *, struct dirtree *, char const *);
void dirtree_dump_tree(struct dirtree const *);
int dirtree_write_tables(struct sqsh_writer *, struct dirtree *);
void dirtree_free(struct dirtree *);
struct dirtree * dirtree_put_reg_for_path(struct sqsh_writer *, struct dirtree *, char const *);
int dirtree_reg_append(struct sqsh_writer *, struct dirtree *, unsigned char const *, size_t);
int dirtree_reg_flush(struct sqsh_writer *, struct dirtree *);

#endif
