/*
Copyright (C) 2018  Charles Cagle

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
#include <vector>

#include "pending_write.h"
#include "sqsh_writer.h"

void pending_write::handle_write()
{
  auto result = future.get();
  report(writer.write_bytes(result.block), result.block, result.compressed);
}

void pending_fragment::report(uint64_t start, std::vector<char> & block,
                              bool compressed)
{
  uint32_t const size = block.size();
  pending_write::writer.push_fragment_entry(
      {start, size | (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT)});
}

void pending_block::report(uint64_t start, std::vector<char> & block,
                           bool compressed)
{
  auto & report = pending_write::writer.reports[inode_number];
  if (report.sizes.empty())
    report.start_block = start;
  report.sizes.push_back(block.size() |
                         (compressed ? 0 : SQFS_BLOCK_COMPRESSED_BIT));
}

void pending_dedup::handle_write()
{
  auto & writer = pending_write::writer;
  auto const sum = writer.blocked_checksums[inode_number].sum;
  auto & reports = writer.reports;
  for (auto i : writer.blocked_duplicates[sum])
    if (reports[inode_number].same_content(reports[i], writer))
      {
        writer.drop_bytes(reports[inode_number].range_len());
        reports[inode_number] = reports[i];
        return;
      }
  writer.blocked_duplicates[sum].push_back(inode_number);
}
