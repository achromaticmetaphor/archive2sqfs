/*
Copyright (C) 2016, 2017, 2018  Charles Cagle

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

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "compressor.h"
#include "endian_buffer.h"
#include "metadata_writer.h"
#include "sqsh_writer.h"

#define MASK_LOW(N) (~((~0u) << (N)))

static void fround_to(std::ostream & f, long int const block)
{
  auto const tell = f.tellp();

  std::size_t const fill = block - (tell % block);
  for (std::size_t i = 0; i < fill; ++i)
    f << '\0';
}

void sqsh_writer::flush_fragment()
{
  if (!current_fragment.empty())
    enqueue_fragment();
}

size_t sqsh_writer::put_fragment()
{
  if (current_fragment.size() + current_block.size() > block_size())
    flush_fragment();

  size_t const offset = current_fragment.size();
  current_fragment.insert(current_fragment.end(), current_block.begin(),
                          current_block.end());
  current_block.clear();
  return offset;
}

void sqsh_writer::write_header()
{
  endian_buffer<SQFS_SUPER_SIZE> header;

  header.l32(SQFS_MAGIC);
  header.l32(next_inode - 1);
  header.l32(0);
  header.l32(1u << super.block_log);
  header.l32(fragments.size());

  header.l16(comp->type);
  header.l16(super.block_log);
  header.l16(super.flags);
  header.l16(ids.size());
  header.l16(SQFS_MAJOR);
  header.l16(SQFS_MINOR);

  header.l64(super.root_inode);
  header.l64(super.bytes_used);
  header.l64(super.id_table_start);
  header.l64(super.xattr_table_start);
  header.l64(super.inode_table_start);
  header.l64(super.directory_table_start);
  header.l64(super.fragment_table_start);
  header.l64(super.lookup_table_start);

  fround_to(outfile, SQFS_PAD_SIZE);
  outfile.seekp(0);
  outfile.write(header.data(), header.size());
}

template <typename T> static constexpr auto ITD_SHIFT(T entry_lb)
{
  return SQFS_META_BLOCK_SIZE_LB - entry_lb;
}

template <typename T> static constexpr auto ITD_MASK(T entry_lb)
{
  return MASK_LOW(ITD_SHIFT(entry_lb));
}

template <typename T> static constexpr auto ITD_ENTRY_SIZE(T entry_lb)
{
  return 1u << entry_lb;
}

template <std::size_t ENTRY_LB, typename G>
static void sqsh_writer_write_indexed_table(sqsh_writer & wr,
                                            std::size_t const count,
                                            uint64_t & table_start, G entry)
{
  endian_buffer<0> indices;
  metadata_writer mdw(*wr.comp);

  for (std::size_t i = 0; i < count; ++i)
    {
      endian_buffer<ITD_ENTRY_SIZE(ENTRY_LB)> buff;
      entry(buff, wr, i);
      meta_address const maddr = mdw.put(buff);

      if ((i & ITD_MASK(ENTRY_LB)) == 0)
        indices.l64(table_start + maddr.block);
    }

  if (count & ITD_MASK(ENTRY_LB))
    mdw.write_block_no_pad();

  mdw.out(wr.outfile);
  table_start = wr.outfile.tellp();

  wr.outfile.write(indices.data(), indices.size());
}

static inline void sqsh_writer_fragment_table_entry(endian_buffer<16> & buff,
                                                    sqsh_writer & wr,
                                                    size_t const i)
{
  fragment_entry const & frag = wr.fragments[i];
  buff.l64(frag.start_block);
  buff.l32(frag.size);
  buff.l32(0);
}

static void sqsh_writer_write_id_table(sqsh_writer & wr)
{
  sqsh_writer_write_indexed_table<2>(
      wr, wr.rids.size(), wr.super.id_table_start,
      [](auto & buff, auto & wr, auto i) { buff.l32(wr.rids[i]); });
}

static void sqsh_writer_write_fragment_table(sqsh_writer & wr)
{
  sqsh_writer_write_indexed_table<4>(wr, wr.fragments.size(),
                                     wr.super.fragment_table_start,
                                     sqsh_writer_fragment_table_entry);
}

static void sqsh_writer_write_inode_table(sqsh_writer & wr)
{
  wr.inode_writer.out(wr.outfile);
}

static void sqsh_writer_write_directory_table(sqsh_writer & wr)
{
  wr.dentry_writer.out(wr.outfile);
}

void sqsh_writer::write_tables()
{
#define TELL_WR(T)                                                           \
  super.T##_table_start = outfile.tellp();                                   \
  sqsh_writer_write_##T##_table(*this);

  TELL_WR(inode);
  TELL_WR(directory);
  TELL_WR(fragment);
  TELL_WR(id);
#undef TELL_WR
  super.bytes_used = outfile.tellp();
}

void sqsh_writer::enqueue_fragment()
{
  if (single_threaded)
    pending_fragment(outfile,
                     comp->compress_async(std::move(current_fragment),
                                          std::launch::deferred),
                     fragments)
        .handle_write();
  else if (!writer_failed)
    writer_queue.push(std::unique_ptr<pending_write>(new pending_fragment(
        outfile,
        comp->compress_async(std::move(current_fragment), std::launch::async),
        fragments)));
  ++fragment_count;
  current_fragment = {};
}

void sqsh_writer::enqueue_block(uint32_t inode_number)
{
  if (single_threaded)
    pending_block(
        outfile,
        comp->compress_async(std::move(current_block), std::launch::deferred),
        inode_number, reports)
        .handle_write();
  else if (!writer_failed)
    writer_queue.push(std::unique_ptr<pending_write>(new pending_block(
        outfile,
        comp->compress_async(std::move(current_block), std::launch::async),
        inode_number, reports)));
  current_block = {};
}

void sqsh_writer::writer_thread()
{
  for (auto option = writer_queue.pop(); option; option = writer_queue.pop())
    try
      {
        if (!writer_failed)
          (*option)->handle_write();
      }
    catch (...)
      {
        writer_failed = true;
      }
}

bool sqsh_writer::finish_data()
{
  flush_fragment();
  writer_queue.finish();
  if (thread.joinable())
    thread.join();
  return writer_failed;
}
