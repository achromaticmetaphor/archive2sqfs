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

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "sqsh_defs.h"
#include "sqsh_writer.h"

struct dirtree : public std::enable_shared_from_this<dirtree>
{
  uint16_t inode_type;
  uint16_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t mtime;
  uint32_t inode_number;
  meta_address inode_address;
  uint32_t nlink;
  uint32_t xattr;
  sqsh_writer * wr;

  dirtree(sqsh_writer * wr, uint16_t inode_type, uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : inode_type(inode_type), mode(mode), uid(uid), gid(gid), mtime(mtime), wr(wr)
  {
    inode_number = wr->next_inode_number();
    nlink = 1;
    xattr = SQFS_XATTR_NONE;
  }

  virtual void dump_tree(std::string const & path) const
  {
    std::cout << path << std::endl;
  }

  void dump_tree() const { dump_tree("."); }

  virtual int write_inode(uint32_t) = 0;
  int write_tables();

  virtual ~dirtree() = default;
};

struct dirtree_ipc : public dirtree
{
  dirtree_ipc(sqsh_writer * wr, uint16_t type, uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : dirtree(wr, type, mode, uid, gid, mtime) {}

  void dump_tree(std::string const & path) const
  {
    std::cout << path << (inode_type == SQFS_INODE_TYPE_PIPE ? "|" : "=") << std::endl;
  }

  virtual int write_inode(uint32_t);
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
  std::size_t block_count;

  dirtree_reg(sqsh_writer * wr, uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : dirtree(wr, SQFS_INODE_TYPE_REG, mode, uid, gid, mtime)
  {
    start_block = 0;
    file_size = 0;
    sparse = 0;
    fragment = SQFS_FRAGMENT_NONE;
    offset = 0;
    block_count = 0;
  }

  void append(unsigned char const *, std::size_t);
  void flush();
  virtual int write_inode(uint32_t);

  template <typename T>
  void append(T & con)
  {
    append(con.data(), con.size());
  }
};

struct dirtree_sym : public dirtree
{
  std::string target;

  dirtree_sym(sqsh_writer * wr, std::string const & target, uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : dirtree(wr, SQFS_INODE_TYPE_SYM, mode, uid, gid, mtime), target(target) {}

  void dump_tree(std::string const & path) const
  {
    std::cout << path << "@ -> " << target << std::endl;
  }

  virtual int write_inode(uint32_t);
};

struct dirtree_dev : public dirtree
{
  uint32_t rdev;

  dirtree_dev(sqsh_writer * wr, uint16_t type, uint32_t rdev, uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : dirtree(wr, type, mode, uid, gid, mtime), rdev(rdev) {}

  void dump_tree(std::string const & path) const
  {
    std::cout << path << (inode_type == SQFS_INODE_TYPE_BLK ? "[]" : "''") << std::endl;
  }

  virtual int write_inode(uint32_t);
};

struct dirtree_dir : public dirtree
{
  std::vector<dirtree_entry> entries;
  uint32_t filesize;
  uint32_t dtable_start_block;
  uint16_t dtable_start_offset;

  dirtree_dir(sqsh_writer * wr, uint16_t mode = 0755, uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0) : dirtree(wr, SQFS_INODE_TYPE_DIR, mode, uid, gid, mtime) {}

  void dump_tree(std::string const & path) const
  {
    std::cout << path << std::endl;
    for (auto & entry : entries)
      entry.inode->dump_tree(path + (entry.inode->inode_type == SQFS_INODE_TYPE_DIR ? "/" : "\t") + entry.name);
  }

  static std::shared_ptr<dirtree_dir> create_root_dir(sqsh_writer * const wr)
  {
    auto const raw_root = new dirtree_dir(wr);
    auto const shared_root = std::shared_ptr<dirtree>(raw_root);
    return std::shared_ptr<dirtree_dir>(shared_root, raw_root);
  }

  std::shared_ptr<dirtree_dir> get_subdir(std::string const &);
  void put_child(std::string const &, std::shared_ptr<dirtree>);
  virtual int write_inode(uint32_t);
  std::shared_ptr<dirtree_dir> subdir_for_path(std::string const &);
  void put_file(std::string const &, std::shared_ptr<dirtree>);
};

#endif
