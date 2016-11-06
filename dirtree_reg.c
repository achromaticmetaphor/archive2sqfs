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
#include "dw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

void dirtree_reg_init(struct dirtree * const dt, struct sqsh_writer * const wr)
{
  dt->inode_type = SQFS_INODE_TYPE_REG;
  dt->mode = 0644;
  dt->uid = 0;
  dt->gid = 0;
  dt->mtime = 0;
  dt->inode_number = sqsh_writer_next_inode_number(wr);

  dt->addi.reg.start_block = 0;
  dt->addi.reg.file_size = 0;
  dt->addi.reg.sparse = 0;
  dt->addi.reg.nlink = 1;
  dt->addi.reg.fragment = UINT32_C(0xffffffff);
  dt->addi.reg.offset = 0;
  dt->addi.reg.xattr = UINT32_C(0xffffffff);

  dt->addi.reg.blocks = NULL;
  dt->addi.reg.nblocks = 0;
  dt->addi.reg.blocks_space = 0;
}

struct dirtree * dirtree_reg_new(struct sqsh_writer * const wr)
{
  struct dirtree * const dt = g_malloc(sizeof(*dt));
  dirtree_reg_init(dt, wr);
  return dt;
}

static void dirtree_reg_add_block(struct dirtree * const dt, size_t size, long int const start_block)
{
  if (dt->addi.reg.nblocks == dt->addi.reg.blocks_space)
    {
      dt->addi.reg.blocks_space += 4;
      dt->addi.reg.blocks = g_realloc(dt->addi.reg.blocks, sizeof(*dt->addi.reg.blocks) * dt->addi.reg.blocks_space);
    }

  dt->addi.reg.blocks[dt->addi.reg.nblocks] = size;
  if (dt->addi.reg.nblocks == 0)
    dt->addi.reg.start_block = start_block;
  dt->addi.reg.nblocks++;
}

void dirtree_reg_flush(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  if (wr->current_pos == 0)
    return;

  if (wr->current_pos < ((size_t) 1 << wr->super.block_log) && dt->addi.reg.nblocks == 0)
    {
      dt->addi.reg.offset = sqsh_writer_put_fragment(wr, wr->current_block, wr->current_pos);
      dt->addi.reg.fragment = wr->super.fragments;
    }
  else
    {
      long int const tell = ftell(wr->outfile);
      uint32_t const bsize = dw_write_data(wr->current_block, wr->current_pos, wr->outfile);
      dirtree_reg_add_block(dt, bsize, tell);
    }
  wr->current_pos = 0;
}

void dirtree_reg_append(struct sqsh_writer * const wr, struct dirtree * const dt, unsigned char const * const buff, size_t const len)
{
  // TODO WHILE len > block_size
  size_t const block_size = (size_t) 1 << wr->super.block_log;
  size_t const remaining = block_size - wr->current_pos;
  size_t const added = len > remaining ? remaining : len;
  dt->addi.reg.file_size += len;

  memcpy(wr->current_block + wr->current_pos, buff, added);
  wr->current_pos += added;
  if (wr->current_pos == block_size)
    {
      dirtree_reg_flush(wr, dt);
      size_t const left = len - added;
      memcpy(wr->current_block, buff + added, left);
      wr->current_pos = left;
    }
}
