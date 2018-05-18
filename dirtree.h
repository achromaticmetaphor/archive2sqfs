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

#ifndef LSL_DIRTREE_H
#define LSL_DIRTREE_H

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sqsh_defs.h"
#include "sqsh_writer.h"

struct dirtree
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

  dirtree(sqsh_writer * wr, uint16_t inode_type, uint16_t mode = 0644,
          uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0)
      : inode_type(inode_type), mode(mode), uid(uid), gid(gid), mtime(mtime),
        wr(wr)
  {
    inode_number = wr->next_inode_number();
    nlink = 1;
    xattr = SQFS_XATTR_NONE;
  }

  template <typename MS> void update_metadata(MS const & ms)
  {
    mode = ms.mode();
    uid = ms.uid();
    gid = ms.gid();
    mtime = ms.mtime();
  }

  virtual void write_inode(uint32_t) = 0;
  void write_tables();

  virtual ~dirtree() = default;
};

struct dirtree_ipc : public dirtree
{
  dirtree_ipc(sqsh_writer * wr, uint16_t type, uint16_t mode = 0644,
              uint32_t uid = 0, uint32_t gid = 0, uint32_t mtime = 0)
      : dirtree(wr, type, mode, uid, gid, mtime)
  {
  }

  virtual void write_inode(uint32_t);
};

struct dirtree_reg : public dirtree
{
  uint64_t file_size;
  uint64_t sparse;

  std::size_t block_count;

  dirtree_reg(sqsh_writer * wr, uint16_t mode = 0644, uint32_t uid = 0,
              uint32_t gid = 0, uint32_t mtime = 0)
      : dirtree(wr, SQFS_INODE_TYPE_REG, mode, uid, gid, mtime)
  {
    file_size = 0;
    sparse = 0;
    block_count = 0;
  }

  void append(char const *, std::size_t);
  void flush();
  void finalize();
  virtual void write_inode(uint32_t);

  template <typename T> void append(T & con)
  {
    append(con.data(), con.size());
  }
};

struct dirtree_sym : public dirtree
{
  std::string target;

  dirtree_sym(sqsh_writer * wr, std::string const & target,
              uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0,
              uint32_t mtime = 0)
      : dirtree(wr, SQFS_INODE_TYPE_SYM, mode, uid, gid, mtime),
        target(target)
  {
  }

  virtual void write_inode(uint32_t);
};

struct dirtree_dev : public dirtree
{
  uint32_t rdev;

  dirtree_dev(sqsh_writer * wr, uint16_t type, uint32_t rdev,
              uint16_t mode = 0644, uint32_t uid = 0, uint32_t gid = 0,
              uint32_t mtime = 0)
      : dirtree(wr, type, mode, uid, gid, mtime), rdev(rdev)
  {
  }

  virtual void write_inode(uint32_t);
};

struct dirtree_dir : public dirtree
{
  std::map<std::string, std::unique_ptr<dirtree>> entries;
  uint32_t filesize;
  uint32_t dtable_start_block;
  uint16_t dtable_start_offset;

  dirtree_dir(sqsh_writer * wr, uint16_t mode = 0755, uint32_t uid = 0,
              uint32_t gid = 0, uint32_t mtime = 0)
      : dirtree(wr, SQFS_INODE_TYPE_DIR, mode, uid, gid, mtime)
  {
  }

  dirtree & put_child(std::string const & name,
                      std::unique_ptr<dirtree> && child)
  {
    return *(entries[name] = std::move(child));
  }

  dirtree_dir & get_subdir(std::string const &);
  virtual void write_inode(uint32_t);
  dirtree_dir & subdir_for_path(std::string const &);
  dirtree & put_file(std::string const &, std::unique_ptr<dirtree> &&);

  template <typename T, typename... A>
  T & put_file(std::string const & name, A... a)
  {
    return static_cast<T &>(put_file(name, std::make_unique<T>(wr, a...)));
  }

  template <typename T, typename MS, typename... A>
  T & put_file_with_metadata(std::string const & name, MS const & ms, A... a)
  {
    return put_file<T>(name, a..., ms.mode(), ms.uid(), ms.gid(), ms.mtime());
  }
};

#endif
