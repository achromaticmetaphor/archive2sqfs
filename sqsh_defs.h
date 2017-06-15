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

#ifndef LSL_SQSH_DEFS_H
#define LSL_SQSH_DEFS_H

#include <cstdint>

#define SQFS_INODE_TYPE_DIR 8
#define SQFS_INODE_TYPE_REG 9
#define SQFS_INODE_TYPE_SYM 10
#define SQFS_INODE_TYPE_BLK 11
#define SQFS_INODE_TYPE_CHR 12
#define SQFS_INODE_TYPE_PIPE 13
#define SQFS_INODE_TYPE_SOCK 14

#define SQFS_MAGIC UINT32_C(0x73717368)
#define SQFS_MAJOR 4
#define SQFS_MINOR 0

#define SQFS_PAD_SIZE 0x1000
#define SQFS_SUPER_SIZE 96

#define SQFS_META_BLOCK_SIZE_LB 13
#define SQFS_META_BLOCK_SIZE (1 << SQFS_META_BLOCK_SIZE_LB)
#define SQFS_META_BLOCK_COMPRESSED_BIT 0x8000u
#define SQFS_BLOCK_COMPRESSED_BIT UINT32_C(0x1000000)
#define SQFS_BLOCK_INVALID 0xffffffff

#define SQFS_XATTR_NONE 0xffffffffu
#define SQFS_FRAGMENT_NONE 0xffffffffu
#define SQFS_TABLE_NOT_PRESENT 0xffffffffffffffffu

#define SQFS_COMPRESSION_TYPE_ZLIB 1

struct meta_address
{
  uint32_t block;
  uint16_t offset;
  uint16_t error;

  meta_address() : block(0), offset(0), error(0) {}
  meta_address(uint16_t e) : block(0), offset(0), error(e) {}
  meta_address(uint32_t b, uint16_t o) : block(b), offset(o), error(0) {}

  operator uint64_t() const
  {
    return (uint64_t(block) << 16) | offset;
  }
};

#endif
