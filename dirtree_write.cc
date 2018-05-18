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

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::literals;

#include "dirtree.h"
#include "endian_buffer.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static bool within16(uint32_t const a, uint32_t const b)
{
  int64_t const diff = (int64_t) b - (int64_t) a;
  return diff < 0x7fff && diff > -0x8000;
}

struct dirtable_header
{
  uint32_t count;
  uint32_t start_block;
  uint32_t inode_number;

  bool works(dirtree const & inode)
  {
    return start_block == inode.inode_address.block &&
           within16(inode_number, inode.inode_number);
  }

  template <typename IT> std::size_t segment_len(IT it, IT const limit)
  {
    size_t i = 0;
    for (; it != limit && works(*it->second); ++i, ++it)
      ;
    return i;
  }
};

template <typename IT>
static void dirtree_write_dirtable_segment(dirtree_dir & dir, IT & it)
{
  auto & first = it->second;
  struct dirtable_header header = {0, first->inode_address.block,
                                   first->inode_number};
  header.count = header.segment_len(it, dir.entries.cend());

  endian_buffer<12> buff;
  buff.l32(header.count - 1);
  buff.l32(header.start_block);
  buff.l32(header.inode_number);
  dir.wr->dentry_writer.put(buff);
  dir.filesize += buff.size();

  for (size_t i = 0; i < header.count; ++i, ++it)
    {
      size_t const len_name = it->first.size();
      if (len_name > 0xff)
        throw std::runtime_error("filename longer than 255 bytes"s);
      endian_buffer<0> buff;

      buff.l16(it->second->inode_address.offset);
      buff.l16(it->second->inode_number - header.inode_number);
      buff.l16(it->second->inode_type - 7);
      buff.l16(len_name - 1);
      for (auto const c : it->first)
        buff.l8(c);

      dir.wr->dentry_writer.put(buff);
      dir.filesize += buff.size();
      if (it->second->inode_type == SQFS_INODE_TYPE_DIR)
        dir.nlink++;
    }
}

static void dirtree_write_dirtable(dirtree_dir & dir)
{
  meta_address const addr = dir.wr->dentry_writer.get_address();
  dir.dtable_start_block = addr.block;
  dir.dtable_start_offset = addr.offset;
  dir.nlink = 2;
  dir.filesize = 3;

  auto it = dir.entries.cbegin();
  while (it != dir.entries.cend())
    dirtree_write_dirtable_segment(dir, it);
}

template <std::size_t N>
static inline void dirtree_inode_common(struct dirtree * const dt,
                                        endian_buffer<N> & buff)
{
  buff.l16(dt->inode_type);
  buff.l16(dt->mode);
  buff.l16(dt->wr->id_lookup(dt->uid));
  buff.l16(dt->wr->id_lookup(dt->gid));
  buff.l32(dt->mtime);
  buff.l32(dt->inode_number);
}

static void dirtree_reg_write_inode_blocks(dirtree_reg & reg)
{
  endian_buffer<0> buff;
  for (auto const b : reg.wr->reports[reg.inode_number].sizes)
    buff.l32(b);
  reg.wr->inode_writer.put(buff);
}

static inline void dirtree_write_inode_dir(endian_buffer<40> & buff,
                                           dirtree_dir & dir,
                                           uint32_t const parent_inode_number)
{
  if (dir.filesize > 0xffffu || dir.xattr != 0xffffffffu)
    {
      buff.l32(dir.nlink);
      buff.l32(dir.filesize);
      buff.l32(dir.dtable_start_block);
      buff.l32(parent_inode_number);
      buff.l16(0);
      buff.l16(dir.dtable_start_offset);
      buff.l32(dir.xattr);
    }
  else
    {
      buff.l16(0, dir.inode_type - 7);
      buff.l32(dir.dtable_start_block);
      buff.l32(dir.nlink);
      buff.l16(dir.filesize);
      buff.l16(dir.dtable_start_offset);
      buff.l32(parent_inode_number);
    }
}

static inline void dirtree_inode_reg(endian_buffer<56> & buff,
                                     dirtree_reg & reg)
{
  auto const start_block = reg.wr->reports[reg.inode_number].start_block;
  auto const findex = reg.wr->fragment_indices[reg.inode_number];
  if (start_block > 0xffffu || reg.file_size > 0xffffu ||
      reg.xattr != 0xffffffffu || reg.nlink != 1)
    {
      buff.l64(start_block);
      buff.l64(reg.file_size);
      buff.l64(reg.sparse);
      buff.l32(reg.nlink);
      buff.l32(findex.fragment);
      buff.l32(findex.offset);
      buff.l32(reg.xattr);
    }
  else
    {
      buff.l16(0, reg.inode_type - 7);
      buff.l32(start_block);
      buff.l32(findex.fragment);
      buff.l32(findex.offset);
      buff.l32(reg.file_size);
    }
}

void dirtree_reg::write_inode(uint32_t)
{
  endian_buffer<56> buff;
  dirtree_inode_common(this, buff);
  dirtree_inode_reg(buff, *this);
  inode_address = wr->inode_writer.put(buff);
  dirtree_reg_write_inode_blocks(*this);
}

void dirtree_dir::write_inode(uint32_t const parent_inode_number)
{
  for (auto & entry : entries)
    entry.second->write_inode(inode_number);

  dirtree_write_dirtable(*this);
  endian_buffer<40> buff;
  dirtree_inode_common(this, buff);
  dirtree_write_inode_dir(buff, *this, parent_inode_number);
  inode_address = wr->inode_writer.put(buff);
}

void dirtree_sym::write_inode(uint32_t)
{
  endian_buffer<0> buff;

  dirtree_inode_common(this, buff);
  buff.l32(nlink);
  buff.l32(target.size());
  for (auto const c : target)
    buff.l8(c);

  if (xattr != SQFS_XATTR_NONE)
    buff.l32(xattr);
  else
    buff.l16(0, inode_type - 7);

  inode_address = wr->inode_writer.put(buff);
}

void dirtree_dev::write_inode(uint32_t)
{
  endian_buffer<28> buff;
  dirtree_inode_common(this, buff);
  buff.l32(nlink);
  buff.l32(rdev);

  if (xattr != SQFS_XATTR_NONE)
    buff.l32(xattr);
  else
    buff.l16(0, inode_type - 7);

  inode_address = wr->inode_writer.put(buff);
}

void dirtree_ipc::write_inode(uint32_t)
{
  endian_buffer<24> buff;
  dirtree_inode_common(this, buff);
  buff.l32(nlink);

  if (xattr != SQFS_XATTR_NONE)
    buff.l32(xattr);
  else
    buff.l16(0, inode_type - 7);

  inode_address = wr->inode_writer.put(buff);
}

void dirtree::write_tables()
{
  write_inode(wr->next_inode);
  wr->super.root_inode = inode_address;
  wr->inode_writer.write_block();
  wr->dentry_writer.write_block();
  wr->write_tables();
}
