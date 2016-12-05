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

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "dirtree.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"
#include "util.h"

static int dirtree_write_dirtable(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  uint64_t const addr = mdw_put(&wr->dentry_writer, NULL, 0);
  RETIF(meta_address_error(addr));

  dt->addi.dir.dtable_start_block = meta_address_block(addr);
  dt->addi.dir.dtable_start_offset = meta_address_offset(addr);
  dt->nlink = 2;
  dt->addi.dir.filesize = 3;

  qsort(dt->addi.dir.entries, dt->addi.dir.nentries, sizeof(*dt->addi.dir.entries), dirtree_entry_compare);
  for (size_t i = 0; i < dt->addi.dir.nentries; i++)
    {
      struct dirtree_entry const * const entry = dt->addi.dir.entries + i;
      size_t const len_name = strlen(entry->name);
      size_t const len_buff = 20 + len_name;
      dt->addi.dir.filesize += len_buff;
      if (entry->inode->inode_type == SQFS_INODE_TYPE_DIR)
        dt->nlink++;

      unsigned char buff[len_buff];
      // TODO: re-use header for later entries where possible
      le32(buff, 0);
      le32(buff + 4, meta_address_block(entry->inode->inode_address));
      le32(buff + 8, entry->inode->inode_number);
      le16(buff + 12, meta_address_offset(entry->inode->inode_address));
      le16(buff + 14, 0);
      le16(buff + 16, entry->inode->inode_type - 7);
      le16(buff + 18, len_name - 1);
      for (size_t i = 0; i < len_name; i++)
        buff[i + 20] = entry->name[i];
      RETIF(meta_address_error(mdw_put(&wr->dentry_writer, buff, len_buff)));
    }

  return 0;
}

static inline void dirtree_inode_common(struct sqsh_writer * const wr, struct dirtree * const dt, unsigned char out[static 16])
{
  le16(out, dt->inode_type);
  le16(out + 2, dt->mode);
  le16(out + 4, sqsh_writer_id_lookup(wr, dt->uid));
  le16(out + 6, sqsh_writer_id_lookup(wr, dt->gid));
  le32(out + 8, dt->mtime);
  le32(out + 12, dt->inode_number);
}

static int dirtree_reg_write_inode_blocks(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  unsigned char buff[dt->addi.reg.nblocks * 4];
  for (size_t i = 0; i < dt->addi.reg.nblocks; i++)
    le32(buff + i * 4, dt->addi.reg.blocks[i]);
  return meta_address_error(mdw_put(&wr->inode_writer, buff, dt->addi.reg.nblocks * 4));
}

static inline size_t dirtree_write_inode_dir(unsigned char buff[static 40], struct dirtree * const dt, uint32_t const parent_inode_number)
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

static inline size_t dirtree_write_inode_reg(unsigned char buff[static 56], struct dirtree * const dt)
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

static int dirtree_write_inode(struct sqsh_writer * const writer, struct dirtree * const dt, uint32_t const parent_inode_number)
{
  if (dt->inode_type == SQFS_INODE_TYPE_DIR)
    for (size_t i = 0; i < dt->addi.dir.nentries; i++)
      RETIF(dirtree_write_inode(writer, dt->addi.dir.entries[i].inode, dt->inode_number));

  _Bool const has_xattr = dt->xattr != 0xffffffffu;
  switch (dt->inode_type)
    {
      case SQFS_INODE_TYPE_DIR:
        {
          RETIF(dirtree_write_dirtable(writer, dt));
          unsigned char buff[40];
          dirtree_inode_common(writer, dt, buff);
          size_t const inode_len = dirtree_write_inode_dir(buff, dt, parent_inode_number);
          dt->inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt->inode_address));
        }
        break;

      case SQFS_INODE_TYPE_REG:
        {
          unsigned char buff[56];
          dirtree_inode_common(writer, dt, buff);
          size_t const inode_len = dirtree_write_inode_reg(buff, dt);
          dt->inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt->inode_address));
          dirtree_reg_write_inode_blocks(writer, dt);
        }
        break;

      case SQFS_INODE_TYPE_SYM:
        {
          size_t const tlen = strlen(dt->addi.sym.target);
          size_t const inode_len = tlen + (has_xattr ? 28 : 24);
          unsigned char buff[inode_len];

          dirtree_inode_common(writer, dt, buff);
          le32(buff + 16, dt->nlink);
          le32(buff + 20, tlen);
          memcpy(buff + 24, dt->addi.sym.target, tlen);

          if (has_xattr)
            le32(buff + 24 + tlen, dt->xattr);
          else
            le16(buff, dt->inode_type - 7);

          dt->inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt->inode_address));
        }
        break;

      case SQFS_INODE_TYPE_BLK:
      case SQFS_INODE_TYPE_CHR:
        {
          size_t const inode_len = has_xattr ? 28 : 24;
          unsigned char buff[inode_len];
          dirtree_inode_common(writer, dt, buff);
          le32(buff + 16, dt->nlink);
          le32(buff + 20, dt->addi.dev.rdev);

          if (has_xattr)
            le32(buff + 24, dt->xattr);
          else
            le16(buff, dt->inode_type - 7);

          dt->inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt->inode_address));
        }
        break;

      case SQFS_INODE_TYPE_PIPE:
      case SQFS_INODE_TYPE_SOCK:
        {
          size_t const inode_len = has_xattr ? 24 : 20;
          unsigned char buff[inode_len];
          dirtree_inode_common(writer, dt, buff);
          le32(buff + 16, dt->nlink);

          if (has_xattr)
            le32(buff + 20, dt->xattr);
          else
            le16(buff, dt->inode_type - 7);

          dt->inode_address = mdw_put(&writer->inode_writer, buff, inode_len);
          RETIF(meta_address_error(dt->inode_address));
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
  RETIF(dirtree_write_inode(wr, dt, wr->next_inode));
  wr->super.root_inode = dt->inode_address;
  RETIF(mdw_write_block(&wr->inode_writer));
  RETIF(mdw_write_block(&wr->dentry_writer));
  RETIF(sqsh_writer_write_tables(wr));
  return 0;
}
