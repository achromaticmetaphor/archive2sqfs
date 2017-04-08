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

#include <iostream>
#include <memory>
#include <string>

#include "dirtree.h"
#include "sqsh_defs.h"

static void dirtree_dump_with_prefix(dirtree_dir const & dir, std::string const & prefix)
{
  std::cout << prefix << std::endl;
  for (auto entry : dir.entries)
    {
      if (entry.inode->inode_type == SQFS_INODE_TYPE_DIR)
        dirtree_dump_with_prefix(*static_cast<dirtree_dir *>(&*entry.inode), prefix + "/" + entry.name);
      else
        {
          std::cout << "\t" << entry.name;

          if (entry.inode->inode_type == SQFS_INODE_TYPE_SYM)
            std::cout << "@ -> " << static_cast<dirtree_sym *>(&*entry.inode)->target;
          else if (entry.inode->inode_type == SQFS_INODE_TYPE_BLK)
            std::cout << "[]";
          else if (entry.inode->inode_type == SQFS_INODE_TYPE_CHR)
            std::cout << "''";
          else if (entry.inode->inode_type == SQFS_INODE_TYPE_PIPE)
            std::cout << "|";
          else if (entry.inode->inode_type == SQFS_INODE_TYPE_SOCK)
            std::cout << "=";

          std::cout << std::endl;
        }
    }
}

void dirtree_dir::dump_tree() const
{
  dirtree_dump_with_prefix(*this, ".");
}
