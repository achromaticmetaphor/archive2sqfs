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

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <search.h>
#include <unistd.h>

#include <glib.h>
#include <zlib.h>

#include "dirtree.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

static int dirtree_entry_compare(void const * const va, void const * const vb)
{
  struct dirtree_entry const * const a = va;
  struct dirtree_entry const * const b = vb;
  return strcmp(a->name, b->name);
}

static void dirtree_dirop_prep(struct dirtree * const dt)
{
  assert(dt->inode_type == SQFS_INODE_TYPE_DIR);

  if (dt->addi.dir.nentries == dt->addi.dir.space)
    {
      dt->addi.dir.space = 2 * dt->addi.dir.space + 1;
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
  dt->inode_number = 0;
  dt->addi.dir.nentries = 0;
  dt->addi.dir.space = 0;
  dt->addi.dir.entries = NULL;
  dt->inode_number = sqsh_writer_next_inode_number(wr);
}

struct dirtree * dirtree_dir_new(struct sqsh_writer * const wr)
{
  struct dirtree * const dt = g_malloc(sizeof(*dt));
  dirtree_dir_init(dt, wr);
  return dt;
}

void dirtree_free(struct dirtree * const dt)
{
  if (dt->inode_type == SQFS_INODE_TYPE_DIR)
    {
      for (size_t i = 0; i < dt->addi.dir.nentries; i++)
        {
          dirtree_free(dt->addi.dir.entries[i].inode);
          g_free((char *) dt->addi.dir.entries[i].name);
        }

      g_free(dt->addi.dir.entries);
    }

  g_free(dt);
}

struct dirtree * dirtree_get_subdir(struct sqsh_writer * const wr, struct dirtree * const dt, char const * name)
{
  dirtree_dirop_prep(dt);
  struct dirtree_entry const e = {name, NULL};
  struct dirtree_entry * const subdir_entry = lsearch(&e, dt->addi.dir.entries, &dt->addi.dir.nentries, sizeof(e), dirtree_entry_compare);
  if (subdir_entry->inode == NULL)
    {
      subdir_entry->name = g_strdup(name);
      subdir_entry->inode = dirtree_dir_new(wr);
    }

  return subdir_entry->inode;
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
    }
}

void dirtree_dump_tree(struct dirtree const * const dt)
{
  assert(dt->inode_type == SQFS_INODE_TYPE_DIR);
  dirtree_dump_with_prefix(dt, ".");
}

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
          le32(buff + 36, 0xffffffff);

          //le32(buff + 16, dt->addi.dir.dtable_start_block);
          //le32(buff + 20, dt->addi.dir.nlink);
          //le16(buff + 24, dt->addi.dir.filesize);
          //le16(buff + 26, dt->addi.dir.dtable_start_offset);
          //le32(buff + 28, parent_inode_number);
          dt->inode_address = mdw_put(&writer->inode_writer, buff, 40);
        }
    }
}

void dirtree_write_tables(struct sqsh_writer * const wr, struct dirtree * const dt)
{
  dirtree_write_inode(wr, dt, wr->next_inode);
  wr->super.root_inode = dt->inode_address;
  mdw_write_block(&wr->inode_writer);
  mdw_write_block(&wr->dentry_writer);
  sqsh_writer_write_tables(wr);
}
