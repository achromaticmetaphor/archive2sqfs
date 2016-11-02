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

#ifndef LSL_SQSH_WRITER_H
#define LSL_SQSH_WRITER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "le.h"
#include "mdw.h"

#define SQFS_BLOCK_LOG_DEFAULT 17

static inline void fround_to(FILE * const f, long int const block)
{
  long int const tell = ftell(f);
  long int const fill = block - (tell % block);
  unsigned char buff[fill];
  memset(buff, 0, fill);
  fwrite(buff, 1, fill, f);
}

struct sqfs_super
{
  //  uint32_t magic;
  //  uint32_t inodes;
  //  uint32_t mkfstime;
  //  uint32_t blocksize;
  uint32_t fragments;

  uint16_t compression;
  uint16_t block_log;
  uint16_t flags;
  uint16_t ids;
  //  uint16_t major;
  //  uint16_t minor;

  uint64_t root_inode;
  uint64_t bytes_used;
  uint64_t id_table_start;
  uint64_t xattr_table_start;
  uint64_t inode_table_start;
  uint64_t directory_table_start;
  uint64_t fragment_table_start;
  uint64_t lookup_table_start;
};

struct sqsh_writer
{
  uint32_t next_inode;
  struct sqfs_super super;
  struct mdw dentry_writer;
  struct mdw inode_writer;
  FILE * outfile;
  unsigned char * current_block;
  size_t current_pos;
};

static inline void sqfs_super_init(struct sqfs_super * const super, int const block_log)
{
  super->fragments = 0;
  super->compression = 1;
  super->block_log = block_log;
  super->flags = 1 << 4;
  super->ids = 1;
  super->root_inode = 0;
  super->bytes_used = 0;
  super->id_table_start = 0;
  super->xattr_table_start = UINT64_C(0xffffffffffffffff);
  super->inode_table_start = 0;
  super->directory_table_start = 0;
  super->fragment_table_start = 0;
  super->lookup_table_start = UINT64_C(0xffffffffffffffff);
}

static inline _Bool sqsh_writer_init(struct sqsh_writer * const wr, char const * const path, int const block_log)
{
  wr->next_inode = 1;
  mdw_init(&wr->dentry_writer);
  mdw_init(&wr->inode_writer);

  sqfs_super_init(&wr->super, block_log);
  wr->current_block = g_malloc(1 << block_log);
  wr->current_pos = 0;

  wr->outfile = fopen(path, "wb");
  return wr->outfile == NULL || fseek(wr->outfile, 96L, SEEK_SET);
}

static inline void sqsh_writer_destroy(struct sqsh_writer * const wr)
{
  if (wr->outfile != NULL)
    fclose(wr->outfile);

  mdw_destroy(&wr->inode_writer);
  mdw_destroy(&wr->dentry_writer);
}

static inline uint32_t sqsh_writer_next_inode_number(struct sqsh_writer * const wr)
{
  return wr->next_inode++;
}

static inline uint16_t sqsh_writer_id_lookup(struct sqsh_writer * const wr, ...)
{
  return 0; // TODO
}

static inline void sqsh_writer_write_header(struct sqsh_writer * const writer)
{
  uint8_t header[96];

  le32(header, SQFS_MAGIC);
  le32(header + 4, writer->next_inode - 1);
  le32(header + 8, 0);
  le32(header + 12, 1u << writer->super.block_log);
  le32(header + 16, writer->super.fragments);

  le16(header + 20, writer->super.compression);
  le16(header + 22, writer->super.block_log);
  le16(header + 24, writer->super.flags);
  le16(header + 26, writer->super.ids);
  le16(header + 28, SQFS_MAJOR);
  le16(header + 30, SQFS_MINOR);

  le64(header + 32, writer->super.root_inode);
  le64(header + 40, writer->super.bytes_used);
  le64(header + 48, writer->super.id_table_start);
  le64(header + 56, writer->super.xattr_table_start);
  le64(header + 64, writer->super.inode_table_start);
  le64(header + 72, writer->super.directory_table_start);
  le64(header + 80, writer->super.fragment_table_start);
  le64(header + 88, writer->super.lookup_table_start);

  fround_to(writer->outfile, SQFS_PAD_SIZE);
  fseek(writer->outfile, 0L, SEEK_SET);
  fwrite(header, sizeof(header), 1, writer->outfile);
}

static inline void sqsh_writer_write_id_table(struct sqsh_writer * const wr)
{
  // TODO
  unsigned char buff[14];
  le16(buff, 0x8004u);
  le32(buff + 2, 250);
  le64(buff + 6, wr->super.id_table_start);
  wr->super.id_table_start += 6;
  fwrite(buff, 1, 14, wr->outfile);
}

static inline int sqsh_writer_write_tables(struct sqsh_writer * const wr)
{
  wr->super.inode_table_start = ftell(wr->outfile);
  mdw_out(&wr->inode_writer, wr->outfile);

  wr->super.directory_table_start = ftell(wr->outfile);
  mdw_out(&wr->dentry_writer, wr->outfile);

  wr->super.fragment_table_start = ftell(wr->outfile);
  wr->super.id_table_start = ftell(wr->outfile);
  sqsh_writer_write_id_table(wr);

  wr->super.bytes_used = ftell(wr->outfile);
  return ferror(wr->outfile);
}

#endif
