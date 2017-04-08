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

#include <memory>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

std::shared_ptr<dirtree> dirtree_sym_new(struct sqsh_writer * const wr)
{
  auto sym = new dirtree_sym;
  dirtree_init(*sym, wr);
  sym->inode_type = SQFS_INODE_TYPE_SYM;
  return std::shared_ptr<dirtree>(sym);
}

std::shared_ptr<dirtree> dirtree_dev_new(struct sqsh_writer * const wr)
{
  auto dev = std::shared_ptr<dirtree>(new dirtree_dev);
  dirtree_init(*dev, wr);
  return dev;
}

std::shared_ptr<dirtree> dirtree_ipc_new(struct sqsh_writer * const wr)
{
  auto ipc = std::shared_ptr<dirtree>(new dirtree);
  dirtree_init(*ipc, wr);
  return ipc;
}
