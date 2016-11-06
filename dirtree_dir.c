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

#include <stdlib.h>
#include <string.h>

#include <search.h>

#include <glib.h>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static void dirtree_dirop_prep(struct dirtree * const dt)
{
  if (dt->addi.dir.nentries == dt->addi.dir.space)
    {
      dt->addi.dir.space += 0x10;
      dt->addi.dir.entries = g_realloc(dt->addi.dir.entries, sizeof(*dt->addi.dir.entries) * dt->addi.dir.space);
    }
}

void dirtree_dir_init(struct dirtree * const dt, struct sqsh_writer * const wr)
{
  dt->inode_type = SQFS_INODE_TYPE_DIR;
  dt->mode = 0755;
  dt->uid = 0;
  dt->gid = 0;
  dt->mtime = 0;
  dt->inode_number = sqsh_writer_next_inode_number(wr);

  dt->addi.dir.nentries = 0;
  dt->addi.dir.space = 0;
  dt->addi.dir.entries = NULL;
}

struct dirtree * dirtree_dir_new(struct sqsh_writer * const wr)
{
  struct dirtree * const dt = g_malloc(sizeof(*dt));
  dirtree_dir_init(dt, wr);
  return dt;
}

static struct dirtree_entry * dirtree_get_child_entry(struct dirtree * const dt, char const * name)
{
  dirtree_dirop_prep(dt);
  struct dirtree_entry const e = {name, NULL};
  return lsearch(&e, dt->addi.dir.entries, &dt->addi.dir.nentries, sizeof(e), dirtree_entry_compare);
}

struct dirtree * dirtree_get_subdir(struct sqsh_writer * const wr, struct dirtree * const dt, char const * name)
{
  struct dirtree_entry * const subdir_entry = dirtree_get_child_entry(dt, name);
  if (subdir_entry->inode == NULL)
    {
      subdir_entry->name = g_strdup(name);
      subdir_entry->inode = dirtree_dir_new(wr);
    }
  // TODO

  return subdir_entry->inode;
}

struct dirtree * dirtree_put_reg(struct sqsh_writer * const wr, struct dirtree * const dt, char const * const name)
{
  struct dirtree_entry * const entry = dirtree_get_child_entry(dt, name);
  if (entry->inode == NULL)
    {
      entry->name = g_strdup(name);
      entry->inode = dirtree_reg_new(wr);
    }
  // TODO
  return entry->inode;
}

struct dirtree * dirtree_get_subdir_for_path(struct sqsh_writer * const wr, struct dirtree * const dt, char const * const path)
{
  char pathtokens[strlen(path) + 1];
  strcpy(pathtokens, path);

  char * ststate;
  struct dirtree * subdir = dt;
  for (char const * component = strtok_r(pathtokens, "/", &ststate); component != NULL; component = strtok_r(NULL, "/", &ststate))
    if (component[0] != 0)
      subdir = dirtree_get_subdir(wr, subdir, component);

  return subdir;
}

struct dirtree * dirtree_put_reg_for_path(struct sqsh_writer * const wr, struct dirtree * const root, char const * const path)
{
  char tmppath[strlen(path) + 1];
  strcpy(tmppath, path);

  char * const sep = strrchr(tmppath, '/');
  char * const name = sep == NULL ? tmppath : sep + 1;
  char * const parent = sep == NULL ? "/" : tmppath;

  if (sep != NULL)
    *sep = 0;

  struct dirtree * parent_dt = dirtree_get_subdir_for_path(wr, root, parent);
  return dirtree_put_reg(wr, parent_dt, name);
}
