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

#include <glib.h>

#include "dirtree.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static void dirtree_write_dirtable(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  uint64_t const addr = mdw_put(&wr->dentry_writer, NULL, 0);
  dt->addi.dir.dtable_start_block = meta_address_block(addr);
  dt->addi.dir.dtable_start_offset = meta_address_offset(addr);
  dt->addi.dir.nlink = 2;
  dt->addi.dir.filesize = 3;

  qsort(dt->addi.dir.entries, dt->addi.dir.nentries, sizeof(*dt->addi.dir.entries), dirtree_entry_compare);
  for (size_t i = 0; i < dt->addi.dir.nentries; i++)
    {
      struct dirtree_entry const * const entry = dt->addi.dir.entries + i;
      size_t const len_name = strlen(entry->name);
      size_t const len_buff = 20 + len_name;
      dt->addi.dir.filesize += len_buff;
      dt->addi.dir.nlink++;

      unsigned char buff[len_buff];
      le32(buff, 0); // TODO
      le32(buff + 4, meta_address_block(entry->inode->inode_address));
      le32(buff + 8, entry->inode->inode_number);
      le16(buff + 12, meta_address_offset(entry->inode->inode_address));
      le16(buff + 14, 0);
      le16(buff + 16, entry->inode->inode_type - 7);
      le16(buff + 18, len_name - 1);
      strncpy(buff + 20, entry->name, len_name);
      mdw_put(&wr->dentry_writer, buff, len_buff);
    }
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

static void dirtree_reg_write_inode_blocks(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  unsigned char buff[dt->addi.reg.nblocks * 4];
  for (size_t i = 0; i < dt->addi.reg.nblocks; i++)
    le32(buff + i * 4, dt->addi.reg.blocks[i]);
  mdw_put(&wr->inode_writer, buff, dt->addi.reg.nblocks * 4);
}

static void dirtree_write_inode(struct sqsh_writer * const writer, struct dirtree * const dt, ino_t const parent_inode_number)
{
  if (dt->inode_type == SQFS_INODE_TYPE_DIR)
    for (size_t i = 0; i < dt->addi.dir.nentries; i++)
      dirtree_write_inode(writer, dt->addi.dir.entries[i].inode, dt->inode_number);

  switch (dt->inode_type)
    {
      case SQFS_INODE_TYPE_DIR:
        {
          dirtree_write_dirtable(writer, dt);
          unsigned char buff[40];
          dirtree_inode_common(writer, dt, buff);
          le32(buff + 16, dt->addi.dir.nlink);
          le32(buff + 20, dt->addi.dir.filesize);
          le32(buff + 24, dt->addi.dir.dtable_start_block);
          le32(buff + 28, parent_inode_number);
          le16(buff + 32, 0);
          le16(buff + 34, dt->addi.dir.dtable_start_offset);
          le32(buff + 36, UINT32_C(0xffffffff));

          //le32(buff + 16, dt->addi.dir.dtable_start_block);
          //le32(buff + 20, dt->addi.dir.nlink);
          //le16(buff + 24, dt->addi.dir.filesize);
          //le16(buff + 26, dt->addi.dir.dtable_start_offset);
          //le32(buff + 28, parent_inode_number);
          dt->inode_address = mdw_put(&writer->inode_writer, buff, 40);
        }
        break;
      case SQFS_INODE_TYPE_REG:
        {
          unsigned char buff[56];
          dirtree_inode_common(writer, dt, buff);
          le64(buff + 16, dt->addi.reg.start_block);
          le64(buff + 24, dt->addi.reg.file_size);
          le64(buff + 32, dt->addi.reg.sparse);
          le32(buff + 40, dt->addi.reg.nlink);
          le32(buff + 44, dt->addi.reg.fragment);
          le32(buff + 48, dt->addi.reg.offset);
          le32(buff + 52, dt->addi.reg.xattr);
          dt->inode_address = mdw_put(&writer->inode_writer, buff, 56);
          dirtree_reg_write_inode_blocks(writer, dt);
        }
        break;
    }
}

void dirtree_write_tables(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  sqsh_writer_flush_fragment(wr);
  dirtree_write_inode(wr, dt, wr->next_inode);
  wr->super.root_inode = dt->inode_address;
  mdw_write_block(&wr->inode_writer);
  mdw_write_block(&wr->dentry_writer);
  sqsh_writer_write_tables(wr);
}
