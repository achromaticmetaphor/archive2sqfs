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

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static void dirtree_sym_init(struct dirtree * const dt, struct sqsh_writer * const wr)
{
  dirtree_init(dt, wr);
  dt->inode_type = SQFS_INODE_TYPE_SYM;
}

struct dirtree * dirtree_sym_new(struct sqsh_writer * const wr)
{
  return dirtree_new(wr, dirtree_sym_init);
}

static void dirtree_untyped_init(struct dirtree * const dt, struct sqsh_writer * const wr)
{
  dirtree_init(dt, wr);
}

struct dirtree * dirtree_dev_new(struct sqsh_writer * const wr)
{
  return dirtree_new(wr, dirtree_untyped_init);
}

struct dirtree * dirtree_ipc_new(struct sqsh_writer * const wr)
{
  return dirtree_new(wr, dirtree_untyped_init);
}
