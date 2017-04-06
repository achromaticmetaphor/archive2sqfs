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
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include <search.h>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static int dirtree_dirop_prep(struct dirtree * const dt)
{
  return 0;
}

void dirtree_dir_init(struct dirtree * const dt, struct sqsh_writer * const wr)
{
  dirtree_init(dt, wr);

  dt->inode_type = SQFS_INODE_TYPE_DIR;
  dt->mode = 0755;

  dt->addi.dir.space = 0;
  dt->addi.dir.entries = new std::vector<dirtree_entry>();
}

struct dirtree * dirtree_dir_new(struct sqsh_writer * const wr)
{
  return dirtree_new(wr, dirtree_dir_init);
}

static struct dirtree_entry * dirtree_get_child_entry(struct dirtree * const dt, char const * name)
{
  auto entry = std::find_if(dt->addi.dir.entries->begin(), dt->addi.dir.entries->end(), [&](auto entry) -> bool { return strcmp(name, entry.name) == 0; });
  if (entry == dt->addi.dir.entries->end())
    {
      dt->addi.dir.entries->push_back({name, nullptr});
      return &dt->addi.dir.entries->back();
    }
  return &*entry;
}

static struct dirtree * dirtree_get_child(struct sqsh_writer * const wr, struct dirtree * const dt, char const * name, struct dirtree * (*con)(struct sqsh_writer *) )
{
  struct dirtree_entry * const entry = dirtree_get_child_entry(dt, name);
  if (entry == nullptr)
    return nullptr;

  if (entry->inode == nullptr)
    {
      entry->name = strdup(name);
      entry->inode = entry->name == nullptr ? nullptr : con(wr);
      if (entry->inode == nullptr)
        {
          free((char *) entry->name);
          dt->addi.dir.entries->pop_back();
          return nullptr;
        }
    }

  return entry->inode;
}

struct dirtree * dirtree_get_subdir(struct sqsh_writer * const wr, struct dirtree * const dt, char const * name)
{
  return dirtree_get_child(wr, dt, name, dirtree_dir_new);
}

struct dirtree * dirtree_put_reg(struct sqsh_writer * const wr, struct dirtree * const dt, char const * const name)
{
  return dirtree_get_child(wr, dt, name, dirtree_reg_new);
}

struct dirtree * dirtree_get_subdir_for_path(struct sqsh_writer * const wr, struct dirtree * const dt, char const * const path)
{
  char pathtokens[strlen(path) + 1];
  strcpy(pathtokens, path);

  char * ststate;
  struct dirtree * subdir = dt;
  for (char const * component = strtok_r(pathtokens, "/", &ststate); component != nullptr; component = strtok_r(nullptr, "/", &ststate))
    if (component[0] != 0 && subdir != nullptr)
      subdir = dirtree_get_subdir(wr, subdir, component);

  return subdir;
}

static struct dirtree * dirtree_put_nondir_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path, struct dirtree * (*con)(struct sqsh_writer *) )
{
  char tmppath[strlen(path) + 1];
  strcpy(tmppath, path);

  char * const sep = strrchr(tmppath, '/');
  char * const name = sep == nullptr ? tmppath : sep + 1;
  char const * const parent = sep == nullptr ? "/" : tmppath;

  if (sep != nullptr)
    *sep = 0;

  struct dirtree * parent_dt = dirtree_get_subdir_for_path(wr, root, parent);
  return parent_dt == nullptr ? nullptr : dirtree_get_child(wr, parent_dt, name, con);
}

struct dirtree * dirtree_put_reg_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path)
{
  return dirtree_put_nondir_for_path(wr, root, path, dirtree_reg_new);
}

struct dirtree * dirtree_put_sym_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path, char const * const target)
{
  struct dirtree * const sym = dirtree_put_nondir_for_path(wr, root, path, dirtree_sym_new);
  if (sym == nullptr)
    return nullptr;

  sym->addi.sym.target = strdup(target);
  if (sym->addi.sym.target == nullptr)
    return dirtree_free(sym), nullptr;

  return sym;
}

struct dirtree * dirtree_put_dev_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path, uint16_t type, uint32_t rdev)
{
  struct dirtree * const dev = dirtree_put_nondir_for_path(wr, root, path, dirtree_dev_new);
  if (dev != nullptr)
    {
      dev->inode_type = type;
      dev->addi.dev.rdev = rdev;
    }

  return dev;
}

struct dirtree * dirtree_put_ipc_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path, uint16_t type)
{
  struct dirtree * const ipc = dirtree_put_nondir_for_path(wr, root, path, dirtree_ipc_new);
  if (ipc != nullptr)
    ipc->inode_type = type;

  return ipc;
}
