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

#define _POSIX_C_SOURCE 200809L

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "dirtree.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"
#include "util.h"

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

  bool works(std::shared_ptr<dirtree> const & inode)
  {
    return start_block == inode->inode_address.block && within16(inode_number, inode->inode_number);
  }

  template <typename IT>
  std::size_t segment_len(IT it, IT const limit)
  {
    size_t i = 0;
    for (; it != limit && works(it->second); ++i, ++it);
    return i;
  }
};

template <typename IT>
static int dirtree_write_dirtable_segment(struct dirtree * const dt, IT & it)
{
  dirtree_dir & dir = *static_cast<dirtree_dir *>(dt);
  auto & first = it->second;
  struct dirtable_header header = {0, first->inode_address.block, first->inode_number};
  header.count = header.segment_len(it, dir.entries.cend());

  unsigned char buff[12];
  le32(buff, header.count - 1);
  le32(buff + 4, header.start_block);
  le32(buff + 8, header.inode_number);
  RETIF(dt->wr->dentry_writer.put(buff, 12).error);
  dir.filesize += 12;

  for (size_t i = 0; i < header.count; ++i, ++it)
    {
      size_t const len_name = it->first.size();
      RETIF(len_name > 0xff);
      std::vector<unsigned char> buff_v(8 + len_name);
      unsigned char * buff = buff_v.data();

      le16(buff, it->second->inode_address.offset);
      le16(buff + 2, it->second->inode_number - header.inode_number);
      le16(buff + 4, it->second->inode_type - 7);
      le16(buff + 6, len_name - 1);
      for (size_t i = 0; i < len_name; i++)
        buff[i + 8] = it->first[i];

      RETIF(dt->wr->dentry_writer.put(buff, 8 + len_name).error);
      dir.filesize += 8 + len_name;
      if (it->second->inode_type == SQFS_INODE_TYPE_DIR)
        dt->nlink++;
    }

  return 0;
}

static int dirtree_write_dirtable(struct dirtree * const dt)
{
  dirtree_dir & dir = *static_cast<dirtree_dir *>(dt);
  meta_address const addr = dt->wr->dentry_writer.get_address();
  RETIF(addr.error);

  dir.dtable_start_block = addr.block;
  dir.dtable_start_offset = addr.offset;
  dt->nlink = 2;
  dir.filesize = 3;

  auto it = dir.entries.cbegin();
  while (it != dir.entries.cend())
    RETIF(dirtree_write_dirtable_segment(dt, it));

  return 0;
}

static inline void dirtree_inode_common(struct dirtree * const dt, unsigned char out[16])
{
  le16(out, dt->inode_type);
  le16(out + 2, dt->mode);
  le16(out + 4, dt->wr->id_lookup(dt->uid));
  le16(out + 6, dt->wr->id_lookup(dt->gid));
  le32(out + 8, dt->mtime);
  le32(out + 12, dt->inode_number);
}

static int dirtree_reg_write_inode_blocks(struct dirtree * const dt)
{
  dirtree_reg & reg = *static_cast<dirtree_reg *>(dt);
  std::vector<unsigned char> buff_v(reg.blocks.size() * 4);
  unsigned char * buff = buff_v.data();
  for (size_t i = 0; i < reg.blocks.size(); i++)
    le32(buff + i * 4, reg.blocks[i]);
  return dt->wr->inode_writer.put(buff, reg.blocks.size() * 4).error;
}

static inline size_t dirtree_write_inode_dir(unsigned char buff[40], struct dirtree * const dt, uint32_t const parent_inode_number)
{
  dirtree_dir & dir = *static_cast<dirtree_dir *>(dt);
  if (dir.filesize > 0xffffu || dt->xattr != 0xffffffffu)
    {
      le32(buff + 16, dt->nlink);
      le32(buff + 20, dir.filesize);
      le32(buff + 24, dir.dtable_start_block);
      le32(buff + 28, parent_inode_number);
      le16(buff + 32, 0);
      le16(buff + 34, dir.dtable_start_offset);
      le32(buff + 36, dt->xattr);
      return 40;
    }
  else
    {
      le16(buff, dt->inode_type - 7);
      le32(buff + 16, dir.dtable_start_block);
      le32(buff + 20, dt->nlink);
      le16(buff + 24, dir.filesize);
      le16(buff + 26, dir.dtable_start_offset);
      le32(buff + 28, parent_inode_number);
      return 32;
    }
}

static inline size_t dirtree_inode_reg(unsigned char buff[56], dirtree_reg & reg)
{
  if (reg.start_block > 0xffffu || reg.file_size > 0xffffu || reg.xattr != 0xffffffffu || reg.nlink != 1)
    {
      le64(buff + 16, reg.start_block);
      le64(buff + 24, reg.file_size);
      le64(buff + 32, reg.sparse);
      le32(buff + 40, reg.nlink);
      le32(buff + 44, reg.fragment);
      le32(buff + 48, reg.offset);
      le32(buff + 52, reg.xattr);
      return 56;
    }
  else
    {
      le16(buff, reg.inode_type - 7);
      le32(buff + 16, reg.start_block);
      le32(buff + 20, reg.fragment);
      le32(buff + 24, reg.offset);
      le32(buff + 28, reg.file_size);
      return 32;
    }
}

int dirtree_reg::write_inode(uint32_t)
{
  unsigned char buff[56];
  dirtree_inode_common(this, buff);
  size_t const inode_len = dirtree_inode_reg(buff, *this);
  inode_address = wr->inode_writer.put(buff, inode_len);
  RETIF(inode_address.error);
  dirtree_reg_write_inode_blocks(this);
  return 0;
}

int dirtree_dir::write_inode(uint32_t const parent_inode_number)
{
  for (auto & entry : entries)
    RETIF(entry.second->write_inode(inode_number));

  RETIF(dirtree_write_dirtable(this));
  unsigned char buff[40];
  dirtree_inode_common(this, buff);
  size_t const inode_len = dirtree_write_inode_dir(buff, this, parent_inode_number);
  inode_address = wr->inode_writer.put(buff, inode_len);
  return inode_address.error;
}

int dirtree_sym::write_inode(uint32_t)
{
  bool const has_xattr = xattr != 0xffffffffu;
  size_t const inode_len = target.size() + (has_xattr ? 28 : 24);
  std::vector<unsigned char> buff(inode_len);

  dirtree_inode_common(this, buff.data());
  le32(buff.data() + 16, nlink);
  le32(buff.data() + 20, target.size());
  for (std::size_t i = 0; i < target.size(); ++i)
    buff[i + 24] = target[i];

  if (has_xattr)
    le32(buff.data() + 24 + target.size(), xattr);
  else
    le16(buff.data(), inode_type - 7);

  inode_address = wr->inode_writer.put(buff.data(), buff.size());
  return inode_address.error;
}

int dirtree_dev::write_inode(uint32_t)
{
  bool const has_xattr = xattr != 0xffffffffu;
  size_t const inode_len = has_xattr ? 28 : 24;
  unsigned char buff[28];
  dirtree_inode_common(this, buff);
  le32(buff + 16, nlink);
  le32(buff + 20, rdev);

  if (has_xattr)
    le32(buff + 24, xattr);
  else
    le16(buff, inode_type - 7);

  inode_address = wr->inode_writer.put(buff, inode_len);
  return inode_address.error;
}

int dirtree_ipc::write_inode(uint32_t)
{
  bool const has_xattr = xattr != 0xffffffffu;
  size_t const inode_len = has_xattr ? 24 : 20;
  unsigned char buff[24];
  dirtree_inode_common(this, buff);
  le32(buff + 16, nlink);

  if (has_xattr)
    le32(buff + 20, xattr);
  else
    le16(buff, inode_type - 7);

  inode_address = wr->inode_writer.put(buff, inode_len);
  return inode_address.error;
}

int dirtree::write_tables()
{
  RETIF(write_inode(wr->next_inode));
  wr->super.root_inode = inode_address;
  wr->inode_writer.write_block();
  wr->dentry_writer.write_block();
  return wr->write_tables();
}
