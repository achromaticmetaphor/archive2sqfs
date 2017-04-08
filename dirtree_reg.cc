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

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "dirtree.h"
#include "dw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"
#include "util.h"

void dirtree_reg_init(dirtree & dt, struct sqsh_writer * const wr)
{
  dirtree_init(dt, wr);

  dirtree_reg & reg = static_cast<dirtree_reg &>(dt);

  dt.inode_type = SQFS_INODE_TYPE_REG;
  reg.start_block = 0;
  reg.file_size = 0;
  reg.sparse = 0;
  reg.fragment = 0xffffffffu;
  reg.offset = 0;

  reg.blocks = new std::vector<uint32_t>();
}

std::shared_ptr<dirtree> dirtree_reg_new(struct sqsh_writer * const wr)
{
  dirtree_reg * reg = new dirtree_reg;
  dirtree_reg_init(*reg, wr);
  return std::shared_ptr<dirtree>(reg);
}

static void dirtree_reg_add_block(struct dirtree * const dt, size_t size, long int const start_block)
{
  dirtree_reg & reg = *static_cast<dirtree_reg *>(dt);
  if (reg.blocks->size() == 0)
    reg.start_block = start_block;

  reg.blocks->push_back(size);
}

int dirtree_reg_flush(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  if (wr->current_block.size() == 0)
    return 0;

  dirtree_reg & reg = *static_cast<dirtree_reg *>(dt);
  size_t const block_size = (size_t) 1 << wr->super.block_log;
  if (wr->current_block.size() < block_size && reg.blocks->size() == 0)
    {
      reg.offset = sqsh_writer_put_fragment(wr, wr->current_block);
      RETIF(reg.offset == block_size);
      reg.fragment = wr->fragments.size();
    }
  else
    {
      long int const tell = ftell(wr->outfile);
      RETIF(tell == -1);

      uint32_t const bsize = dw_write_data(wr->current_block, wr->outfile);
      RETIF(bsize == 0xffffffff);

      dirtree_reg_add_block(dt, bsize, tell);
    }

  wr->current_block.clear();
  return 0;
}

int dirtree_reg_append(struct sqsh_writer * const wr, struct dirtree * const dt, unsigned char const * buff, size_t len)
{
  dirtree_reg & reg = *static_cast<dirtree_reg *>(dt);
  reg.file_size += len;
  size_t const block_size = (size_t) 1 << wr->super.block_log;
  while (len != 0)
    {
      size_t const remaining = block_size - wr->current_block.size();
      size_t const added = len > remaining ? remaining : len;
      for (std::size_t i = 0; i < added; ++i)
        wr->current_block.push_back(buff[i]);

      if (wr->current_block.size() == block_size)
        RETIF(dirtree_reg_flush(wr, dt));

      len -= added;
      buff += added;
    }

  return 0;
}
