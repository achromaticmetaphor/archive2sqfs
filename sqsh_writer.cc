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

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "compressor.h"
#include "le.h"
#include "mdw.h"
#include "sqsh_writer.h"
#include "util.h"

static int fround_to(std::ostream & f, long int const block)
{
  auto const tell = f.tellp();
  if (tell == decltype(tell)(-1))
    return 1;

  std::size_t const fill = block - (tell % block);
  for (std::size_t i = 0; i < fill; ++i)
    f << '\0';
  return f.fail();
}

void sqsh_writer::flush_fragment()
{
  if (!current_fragment.empty())
    enqueue_fragment();
}

size_t sqsh_writer::put_fragment()
{
  size_t const block_size = (size_t) 1 << super.block_log;
  if (current_fragment.size() + current_block.size() > block_size)
    flush_fragment();

  size_t const offset = current_fragment.size();
  current_fragment.insert(current_fragment.end(), current_block.begin(), current_block.end());
  current_block.clear();
  return offset;
}

int sqsh_writer::write_header()
{
  uint8_t header[SQFS_SUPER_SIZE];

  le32(header, SQFS_MAGIC);
  le32(header + 4, next_inode - 1);
  le32(header + 8, 0);
  le32(header + 12, 1u << super.block_log);
  le32(header + 16, fragments.size());

  le16(header + 20, comp->type);
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

  RETIF(fround_to(outfile, SQFS_PAD_SIZE));
  outfile.seekp(0);
  outfile.write(reinterpret_cast<char *>(header), SQFS_SUPER_SIZE);
  return outfile.fail();
}

template <typename T>
static constexpr auto ITD_SHIFT(T entry_lb)
{
  return SQFS_META_BLOCK_SIZE_LB - entry_lb;
}

template <typename T>
static constexpr auto ITD_MASK(T entry_lb)
{
  return MASK_LOW(ITD_SHIFT(entry_lb));
}

template <typename T>
static constexpr auto ITD_ENTRY_SIZE(T entry_lb)
{
  return 1u << entry_lb;
}

template <std::size_t ENTRY_LB, typename G>
static int sqsh_writer_write_indexed_table(sqsh_writer * wr, std::size_t const count, uint64_t & table_start, G entry)
{
  std::size_t const index_count = (count >> ITD_SHIFT(ENTRY_LB)) + ((count & ITD_MASK(ENTRY_LB)) != 0);
  std::vector<unsigned char> indices(index_count * 8);
  mdw mdw(*wr->comp);
  std::size_t index = 0;

  for (std::size_t i = 0; i < count; ++i)
    {
      unsigned char buff[ITD_ENTRY_SIZE(ENTRY_LB)];
      entry(buff, wr, i);
      meta_address const maddr = mdw.put(buff, ITD_ENTRY_SIZE(ENTRY_LB));
      RETIF(maddr.error);

      if ((i & ITD_MASK(ENTRY_LB)) == 0)
        le64(indices.data() + index++ * 8, table_start + maddr.block);
    }

  int error = 0;
  if (count & ITD_MASK(ENTRY_LB))
    mdw.write_block_no_pad();

  error = error || mdw.out(wr->outfile);

  auto const tell = wr->outfile.tellp();
  error = error || tell == decltype(tell)(-1);
  table_start = tell;

  RETIF(error);
  wr->outfile.write(reinterpret_cast<char *>(indices.data()), indices.size());
  return wr->outfile.fail();
}

static inline void sqsh_writer_fragment_table_entry(unsigned char buff[16], struct sqsh_writer * const wr, size_t const i)
{
  fragment_entry const & frag = wr->fragments[i];
  le64(buff, frag.start_block);
  le32(buff + 8, frag.size);
  le32(buff + 12, 0);
}

static int sqsh_writer_write_id_table(sqsh_writer * wr)
{
  return sqsh_writer_write_indexed_table<2>(wr, wr->rids.size(), wr->super.id_table_start, [](auto buff, auto wr, auto i) { le32(buff, wr->rids[i]); });
}

static int sqsh_writer_write_fragment_table(sqsh_writer * wr)
{
  return sqsh_writer_write_indexed_table<4>(wr, wr->fragments.size(), wr->super.fragment_table_start, sqsh_writer_fragment_table_entry);
}

static int sqsh_writer_write_inode_table(struct sqsh_writer * const wr)
{
  return wr->inode_writer.out(wr->outfile);
}

static int sqsh_writer_write_directory_table(struct sqsh_writer * const wr)
{
  return wr->dentry_writer.out(wr->outfile);
}

static inline void tell_wr(struct sqsh_writer * const wr, int & error, uint64_t & start, int (*cb)(struct sqsh_writer *))
{
  auto const tell = wr->outfile.tellp();
  error = error || tell == decltype(tell)(-1);
  start = tell;
  error = error || cb(wr);
}

int sqsh_writer::write_tables()
{
  int error = 0;

#define TELL_WR(T) tell_wr(this, error, super.T##_table_start, sqsh_writer_write_##T##_table)
  TELL_WR(inode);
  TELL_WR(directory);
  TELL_WR(fragment);
  TELL_WR(id);
#undef TELL_WR

  long int const tell = outfile.tellp();
  error = error || tell == decltype(tell)(-1);
  super.bytes_used = tell;

  return error;
}

void sqsh_writer::enqueue_fragment()
{
  writer_queue.push(std::unique_ptr<pending_write>(new pending_fragment(outfile, comp->compress_async(std::move(current_fragment)), fragments)));
  ++fragment_count;
  current_fragment = {};
}

void sqsh_writer::enqueue_block(std::shared_ptr<std::vector<uint32_t>> blocks, std::shared_ptr<uint64_t> start)
{
  writer_queue.push(std::unique_ptr<pending_write>(new pending_block(outfile, comp->compress_async(std::move(current_block)), blocks, start)));
  current_block = {};
}

void sqsh_writer::writer_thread()
{
  bool failed = false;
  for (auto option = writer_queue.pop(); option; option = writer_queue.pop())
    failed = failed || (*option)->handle_write();
  writer_failed = failed;
}

bool sqsh_writer::finish_data()
{
  flush_fragment();
  writer_queue.finish();
  thread.join();
  return writer_failed;
}
