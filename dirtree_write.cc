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

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <memory>

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

  bool works(dirtree_entry & entry)
  {
    return start_block == meta_address_block(entry.inode->inode_address) && within16(inode_number, entry.inode->inode_number);
  }
};

static size_t dirtree_dirtable_segment_len(struct dirtree * const dt, struct dirtable_header * const header, size_t const offset)
{
  for (size_t i = 0; i < dt->addi.dir.entries->size() - offset; ++i)
    if (!header->works((*dt->addi.dir.entries)[offset + i]))
      return i;
  return dt->addi.dir.entries->size() - offset;
}

static int dirtree_write_dirtable_segment(struct sqsh_writer * const wr, struct dirtree * const dt, size_t * const offset)
{
  std::shared_ptr<dirtree const> const first = (*dt->addi.dir.entries)[*offset].inode;
  struct dirtable_header header = {0, meta_address_block(first->inode_address), first->inode_number};
  header.count = dirtree_dirtable_segment_len(dt, &header, *offset);

  unsigned char buff[12];
  le32(buff, header.count - 1);
  le32(buff + 4, header.start_block);
  le32(buff + 8, header.inode_number);
  RETIF(meta_address_error(mdw_put(&wr->dentry_writer, buff, 12)));
  dt->addi.dir.filesize += 12;

  for (size_t i = 0; i < header.count; i++)
    {
      dirtree_entry const & entry = (*dt->addi.dir.entries)[*offset + i];
      size_t const len_name = strlen(entry.name);
      RETIF(len_name > 0xff);
      unsigned char buff[8 + len_name];

      le16(buff, meta_address_offset(entry.inode->inode_address));
      le16(buff + 2, entry.inode->inode_number - header.inode_number);
      le16(buff + 4, entry.inode->inode_type - 7);
      le16(buff + 6, len_name - 1);
      for (size_t i = 0; i < len_name; i++)
        buff[i + 8] = entry.name[i];

      RETIF(meta_address_error(mdw_put(&wr->dentry_writer, buff, 8 + len_name)));
      dt->addi.dir.filesize += 8 + len_name;
      if (entry.inode->inode_type == SQFS_INODE_TYPE_DIR)
        dt->nlink++;
    }

  *offset += header.count;
  return 0;
}

static int dirtree_write_dirtable(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  uint64_t const addr = mdw_put(&wr->dentry_writer, nullptr, 0);
  RETIF(meta_address_error(addr));

  dt->addi.dir.dtable_start_block = meta_address_block(addr);
  dt->addi.dir.dtable_start_offset = meta_address_offset(addr);
  dt->nlink = 2;
  dt->addi.dir.filesize = 3;

  std::sort(dt->addi.dir.entries->begin(), dt->addi.dir.entries->end(), [](auto a, auto b) -> bool { return strcmp(a.name, b.name) < 0; });
  size_t offset = 0;
  while (offset < dt->addi.dir.entries->size())
    RETIF(dirtree_write_dirtable_segment(wr, dt, &offset));

  return 0;
}

static inline void dirtree_inode_common(struct sqsh_writer * const wr, struct dirtree * const dt, unsigned char out[16])
{
  le16(out, dt->inode_type);
  le16(out + 2, dt->mode);
  le16(out + 4, wr->id_lookup(dt->uid));
  le16(out + 6, wr->id_lookup(dt->gid));
  le32(out + 8, dt->mtime);
  le32(out + 12, dt->inode_number);
}

static int dirtree_reg_write_inode_blocks(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  unsigned char buff[dt->addi.reg.blocks->size() * 4];
  for (size_t i = 0; i < dt->addi.reg.blocks->size(); i++)
    le32(buff + i * 4, (*dt->addi.reg.blocks)[i]);
  return meta_address_error(mdw_put(&wr->inode_writer, buff, dt->addi.reg.blocks->size() * 4));
}

static inline size_t dirtree_write_inode_dir(unsigned char buff[40], struct dirtree * const dt, uint32_t const parent_inode_number)
{
  if (dt->addi.dir.filesize > 0xffffu || dt->xattr != 0xffffffffu)
    {
      le32(buff + 16, dt->nlink);
      le32(buff + 20, dt->addi.dir.filesize);
      le32(buff + 24, dt->addi.dir.dtable_start_block);
      le32(buff + 28, parent_inode_number);
      le16(buff + 32, 0);
      le16(buff + 34, dt->addi.dir.dtable_start_offset);
      le32(buff + 36, dt->xattr);
      return 40;
    }
  else
    {
      le16(buff, dt->inode_type - 7);
      le32(buff + 16, dt->addi.dir.dtable_start_block);
      le32(buff + 20, dt->nlink);
      le16(buff + 24, dt->addi.dir.filesize);
      le16(buff + 26, dt->addi.dir.dtable_start_offset);
      le32(buff + 28, parent_inode_number);
      return 32;
    }
}

static inline size_t dirtree_write_inode_reg(unsigned char buff[56], struct dirtree * const dt)
{
  if (dt->addi.reg.start_block > 0xffffu || dt->addi.reg.file_size > 0xffffu || dt->xattr != 0xffffffffu || dt->nlink != 1)
    {
      le64(buff + 16, dt->addi.reg.start_block);
      le64(buff + 24, dt->addi.reg.file_size);
      le64(buff + 32, dt->addi.reg.sparse);
      le32(buff + 40, dt->nlink);
      le32(buff + 44, dt->addi.reg.fragment);
      le32(buff + 48, dt->addi.reg.offset);
      le32(buff + 52, dt->xattr);
      return 56;
    }
  else
    {
      le16(buff, dt->inode_type - 7);
      le32(buff + 16, dt->addi.reg.start_block);
      le32(buff + 20, dt->addi.reg.fragment);
      le32(buff + 24, dt->addi.reg.offset);
      le32(buff + 28, dt->addi.reg.file_size);
      return 32;
    }
}

static int dirtree_write_inode(struct sqsh_writer * const writer, dirtree & dt, uint32_t const parent_inode_number)
{
  if (dt.inode_type == SQFS_INODE_TYPE_DIR)
    for (auto entry : *dt.addi.dir.entries)
      RETIF(dirtree_write_inode(writer, *entry.inode, dt.inode_number));

  bool const has_xattr = dt.xattr != 0xffffffffu;
  switch (dt.inode_type)
    {
      case SQFS_INODE_TYPE_DIR:
        {
          RETIF(dirtree_write_dirtable(writer, &dt));
          unsigned char buff[40];
          dirtree_inode_common(writer, &dt, buff);
          size_t const inode_len = dirtree_write_inode_dir(buff, &dt, parent_inode_number);
          dt.inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt.inode_address));
        }
        break;

      case SQFS_INODE_TYPE_REG:
        {
          unsigned char buff[56];
          dirtree_inode_common(writer, &dt, buff);
          size_t const inode_len = dirtree_write_inode_reg(buff, &dt);
          dt.inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt.inode_address));
          dirtree_reg_write_inode_blocks(writer, &dt);
        }
        break;

      case SQFS_INODE_TYPE_SYM:
        {
          size_t const tlen = strlen(dt.addi.sym.target);
          size_t const inode_len = tlen + (has_xattr ? 28 : 24);
          unsigned char buff[inode_len];

          dirtree_inode_common(writer, &dt, buff);
          le32(buff + 16, dt.nlink);
          le32(buff + 20, tlen);
          memcpy(buff + 24, dt.addi.sym.target, tlen);

          if (has_xattr)
            le32(buff + 24 + tlen, dt.xattr);
          else
            le16(buff, dt.inode_type - 7);

          dt.inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt.inode_address));
        }
        break;

      case SQFS_INODE_TYPE_BLK:
      case SQFS_INODE_TYPE_CHR:
        {
          size_t const inode_len = has_xattr ? 28 : 24;
          unsigned char buff[inode_len];
          dirtree_inode_common(writer, &dt, buff);
          le32(buff + 16, dt.nlink);
          le32(buff + 20, dt.addi.dev.rdev);

          if (has_xattr)
            le32(buff + 24, dt.xattr);
          else
            le16(buff, dt.inode_type - 7);

          dt.inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt.inode_address));
        }
        break;

      case SQFS_INODE_TYPE_PIPE:
      case SQFS_INODE_TYPE_SOCK:
        {
          size_t const inode_len = has_xattr ? 24 : 20;
          unsigned char buff[inode_len];
          dirtree_inode_common(writer, &dt, buff);
          le32(buff + 16, dt.nlink);

          if (has_xattr)
            le32(buff + 20, dt.xattr);
          else
            le16(buff, dt.inode_type - 7);

          dt.inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt.inode_address));
        }
        break;

      default:
        return 1;
    }

  return 0;
}

int dirtree_write_tables(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  RETIF(sqsh_writer_flush_fragment(wr));
  RETIF(dirtree_write_inode(wr, *dt, wr->next_inode));
  wr->super.root_inode = dt->inode_address;
  mdw_write_block(&wr->inode_writer);
  mdw_write_block(&wr->dentry_writer);
  RETIF(sqsh_writer_write_tables(wr));
  return 0;
}
