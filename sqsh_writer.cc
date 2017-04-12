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
#include <stdint.h>
#include <stdio.h>

#include <search.h>

#include "dw.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_writer.h"
#include "util.h"

static int fround_to(FILE * const f, long int const block)
{
  long int const tell = ftell(f);
  if (tell == -1)
    return 1;

  size_t const fill = block - (tell % block);
  unsigned char buff[fill];
  for (auto i = 0; i < fill; ++i)
    buff[i] = 0;
  return fwrite(buff, 1, fill, f) != fill;
}

static void sqsh_writer_append_fragment(struct sqsh_writer * const wr, uint32_t const size, uint64_t const start_block)
{
  wr->fragments.push_back({start_block, size});
}

int sqsh_writer::flush_fragment()
{
  if (current_fragment.size() == 0)
    return 0;

  long int const tell = ftell(outfile);
  if (tell == -1)
    return 1;

  uint32_t const bsize = dw_write_data(current_fragment, outfile);
  if (bsize == 0xffffffff)
    return 1;

  current_fragment.clear();
  sqsh_writer_append_fragment(this, bsize, tell);
  return 0;
}

size_t sqsh_writer::put_fragment()
{
  size_t const block_size = (size_t) 1 << super.block_log;

  if (current_fragment.size() + current_block.size() > block_size)
    if (flush_fragment())
      return block_size;

  size_t const offset = current_fragment.size();
  for (auto e : current_block)
    current_fragment.push_back(e);
  return offset;
}

int sqsh_writer::write_header()
{
  uint8_t header[96];

  le32(header, SQFS_MAGIC);
  le32(header + 4, next_inode - 1);
  le32(header + 8, 0);
  le32(header + 12, 1u << super.block_log);
  le32(header + 16, fragments.size());

  le16(header + 20, super.compression);
  le16(header + 22, super.block_log);
  le16(header + 24, super.flags);
  le16(header + 26, ids.size());
  le16(header + 28, SQFS_MAJOR);
  le16(header + 30, SQFS_MINOR);

  le64(header + 32, super.root_inode);
  le64(header + 40, super.bytes_used);
  le64(header + 48, super.id_table_start);
  le64(header + 56, super.xattr_table_start);
  le64(header + 64, super.inode_table_start);
  le64(header + 72, super.directory_table_start);
  le64(header + 80, super.fragment_table_start);
  le64(header + 88, super.lookup_table_start);

  return fround_to(outfile, SQFS_PAD_SIZE) || fseek(outfile, 0L, SEEK_SET) || fwrite(header, 1, sizeof(header), outfile) != sizeof(header);
}

#define ITD_SHIFT(ENTRY_LB) (SQFS_META_BLOCK_SIZE_LB - (ENTRY_LB))
#define ITD_MASK(ENTRY_LB) MASK_LOW(ITD_SHIFT(ENTRY_LB))
#define ITD_ENTRY_SIZE(ENTRY_LB) (1u << (ENTRY_LB))

#define SQSH_WRITER_WRITE_INDEXED_TABLE_DEFN(TABLE, ENTRY_LB, COUNT_FIELD)                                              \
  static int sqsh_writer_write_##TABLE##_table(struct sqsh_writer * const wr)                                           \
  {                                                                                                                     \
    size_t const index_count = (wr->COUNT_FIELD >> ITD_SHIFT(ENTRY_LB)) + ((wr->ids.size() & ITD_MASK(ENTRY_LB)) != 0); \
    unsigned char indices[index_count * 8];                                                                             \
    struct mdw mdw;                                                                                                     \
    size_t index = 0;                                                                                                   \
                                                                                                                        \
    for (size_t i = 0; i < wr->COUNT_FIELD; i++)                                                                        \
      {                                                                                                                 \
        unsigned char buff[ITD_ENTRY_SIZE(ENTRY_LB)];                                                                   \
        sqsh_writer_##TABLE##_table_entry(buff, wr, i);                                                                 \
        uint64_t const maddr = mdw.put(buff, ITD_ENTRY_SIZE(ENTRY_LB));                                                 \
        RETIF(meta_address_error(maddr));                                                                               \
                                                                                                                        \
        if ((i & ITD_MASK(ENTRY_LB)) == 0)                                                                              \
          le64(indices + index++ * 8, wr->super.TABLE##_table_start + meta_address_block(maddr));                       \
      }                                                                                                                 \
                                                                                                                        \
    int error = 0;                                                                                                      \
    if (wr->COUNT_FIELD & ITD_MASK(ENTRY_LB))                                                                           \
      mdw.write_block_no_pad();                                                                                         \
                                                                                                                        \
    error = error || mdw.out(wr->outfile);                                                                              \
                                                                                                                        \
    long int const tell = error ? -1 : ftell(wr->outfile);                                                              \
    error = error || tell == -1;                                                                                        \
    wr->super.TABLE##_table_start = tell;                                                                               \
                                                                                                                        \
    return error || fwrite(indices, 1, index_count * 8, wr->outfile) != index_count * 8;                                \
  }

static inline void sqsh_writer_id_table_entry(unsigned char buff[4], struct sqsh_writer * const wr, size_t const i)
{
  le32(buff, wr->rids[i]);
}

static inline void sqsh_writer_fragment_table_entry(unsigned char buff[16], struct sqsh_writer * const wr, size_t const i)
{
  fragment_entry const & frag = wr->fragments[i];
  le64(buff, frag.start_block);
  le32(buff + 8, frag.size);
  le32(buff + 12, 0);
}

SQSH_WRITER_WRITE_INDEXED_TABLE_DEFN(id, 2, rids.size())
SQSH_WRITER_WRITE_INDEXED_TABLE_DEFN(fragment, 4, fragments.size())

static int sqsh_writer_write_inode_table(struct sqsh_writer * const wr)
{
  return wr->inode_writer.out(wr->outfile);
}

static int sqsh_writer_write_directory_table(struct sqsh_writer * const wr)
{
  return wr->dentry_writer.out(wr->outfile);
}

static inline void tell_wr(struct sqsh_writer * const wr, int * const error, uint64_t * const start, int (*cb)(struct sqsh_writer *))
{
  long int const tell = ftell(wr->outfile);
  *error = *error || tell == -1;
  *start = tell;
  *error = *error || cb(wr);
}

int sqsh_writer::write_tables()
{
  int error = 0;

#define TELL_WR(T) tell_wr(this, &error, &super.T##_table_start, sqsh_writer_write_##T##_table)
  TELL_WR(inode);
  TELL_WR(directory);
  TELL_WR(fragment);
  TELL_WR(id);
#undef TELL_WR

  long int const tell = ftell(outfile);
  error = error || tell == -1;
  super.bytes_used = tell;

  return error;
}
