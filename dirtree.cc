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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirtree.h"
#include "sqsh_defs.h"

int dirtree_entry_compare(void const * const va, void const * const vb)
{
  return strcmp(reinterpret_cast<dirtree_entry const *>(va)->name, reinterpret_cast<dirtree_entry const *>(vb)->name);
}

int dirtree_entry_by_name(void const * const va, void const * const vb)
{
  return strcmp(reinterpret_cast<char const *>(va), reinterpret_cast<dirtree_entry const *>(vb)->name);
}

void dirtree_free(struct dirtree * const dt)
{
  if (dt->inode_type == SQFS_INODE_TYPE_DIR)
    {
      for (size_t i = 0; i < dt->addi.dir.nentries; i++)
        {
          dirtree_free(dt->addi.dir.entries[i].inode);
          free((char *) dt->addi.dir.entries[i].name);
        }

      free(dt->addi.dir.entries);
    }
  else if (dt->inode_type == SQFS_INODE_TYPE_REG)
    free(dt->addi.reg.blocks);
  else if (dt->inode_type == SQFS_INODE_TYPE_SYM)
    free(dt->addi.sym.target);

  free(dt);
}

static void dirtree_dump_with_prefix(struct dirtree const * const dt, char const * const prefix)
{
  puts(prefix);
  for (size_t i = 0; i < dt->addi.dir.nentries; i++)
    {
      struct dirtree_entry const * const entry = dt->addi.dir.entries + i;
      if (entry->inode->inode_type == SQFS_INODE_TYPE_DIR)
        {
          char prefix_[strlen(prefix) + strlen(entry->name) + 2];
          sprintf(prefix_, "%s/%s", prefix, entry->name);
          dirtree_dump_with_prefix(entry->inode, prefix_);
        }
      if (entry->inode->inode_type == SQFS_INODE_TYPE_REG)
        printf("\t%s\n", entry->name);
    }
}

void dirtree_dump_tree(struct dirtree const * const dt)
{
  dirtree_dump_with_prefix(dt, ".");
}
