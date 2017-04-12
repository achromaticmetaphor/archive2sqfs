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

  dirtree(sqsh_writer * wr)
  {
    mode = 0644;
    uid = 0;
    gid = 0;
    mtime = 0;
    inode_number = wr->next_inode_number();
    nlink = 1;
    xattr = 0xffffffffu;
  }

  virtual int write_inode(sqsh_writer *, uint32_t) = 0;
  int write_tables(struct sqsh_writer *);

  virtual ~dirtree() = default;
};

struct dirtree_ipc : public dirtree
{
  dirtree_ipc(sqsh_writer * wr, uint16_t type) : dirtree(wr)
  {
    inode_type = type;
  }

  virtual int write_inode(sqsh_writer *, uint32_t);
};

struct dirtree_entry
{
  std::string name;
  std::shared_ptr<dirtree> inode;
};

struct dirtree_reg : public dirtree
{
  uint64_t start_block;
  uint64_t file_size;
  uint64_t sparse;
  uint32_t fragment;
  uint32_t offset;

  std::vector<uint32_t> blocks;

  dirtree_reg(sqsh_writer * wr) : dirtree(wr)
  {
    inode_type = SQFS_INODE_TYPE_REG;
    start_block = 0;
    file_size = 0;
    sparse = 0;
    fragment = 0xffffffffu;
    offset = 0;
  }

  void add_block(std::size_t, long int);
  int append(sqsh_writer *, unsigned char const *, std::size_t);
  int flush(sqsh_writer *);
  virtual int write_inode(sqsh_writer *, uint32_t);
};

struct dirtree_sym : public dirtree
{
  std::string target;

  dirtree_sym(sqsh_writer * wr, std::string const & target) : dirtree(wr), target(target)
  {
    inode_type = SQFS_INODE_TYPE_SYM;
  }

  virtual int write_inode(sqsh_writer *, uint32_t);
};

struct dirtree_dev : public dirtree
{
  uint32_t rdev;

  dirtree_dev(sqsh_writer * wr, uint16_t type, uint32_t rdev) : dirtree(wr), rdev(rdev)
  {
    inode_type = type;
  }

  virtual int write_inode(sqsh_writer *, uint32_t);
};

struct dirtree_dir : public dirtree
{
  std::vector<dirtree_entry> entries;
  uint32_t filesize;
  uint32_t dtable_start_block;
  uint16_t dtable_start_offset;

  dirtree_dir(sqsh_writer * wr) : dirtree(wr)
  {
    inode_type = SQFS_INODE_TYPE_DIR;
    mode = 0755;
  }

  void dump_tree() const;
  virtual int write_inode(sqsh_writer *, uint32_t);
  dirtree_dir * subdir_for_path(sqsh_writer *, std::string const &);
  dirtree_reg * put_reg(sqsh_writer *, std::string const &);
  dirtree_sym * put_sym(sqsh_writer *, std::string const &, std::string const &);
  dirtree_dev * put_dev(sqsh_writer *, std::string const &, uint16_t, uint32_t);
  dirtree_ipc * put_ipc(sqsh_writer *, std::string const &, uint16_t);
};

#endif
