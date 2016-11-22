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

#ifndef LSL_SQSH_DEFS_H
#define LSL_SQSH_DEFS_H

#include <stdint.h>

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

#define SQFS_META_BLOCK_SIZE_LB 13
#define SQFS_META_BLOCK_SIZE (1 << SQFS_META_BLOCK_SIZE_LB)
#define SQFS_META_BLOCK_COMPRESSED_BIT 0x8000u
#define SQFS_BLOCK_COMPRESSED_BIT UINT32_C(0x1000000)

static inline uint16_t meta_address_offset(uint64_t const maddr)
{
  return maddr & 0xffffu;
}

static inline uint32_t meta_address_block(uint64_t const maddr)
{
  return (maddr >> 16) & 0xffffffffu;
}

static inline uint64_t meta_address(uint32_t const block, uint16_t const offset)
{
  return (((uint64_t) block) << 16) | offset;
}

static inline uint64_t meta_address_from_error(uint16_t const error)
{
  return (uint64_t) error << 48;
}

static inline uint16_t meta_address_error(uint64_t const maddr)
{
  return maddr >> 48;
}

#endif
