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

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

std::shared_ptr<dirtree_dir> dirtree_dir::subdir_for_path(std::string const & path)
{
  auto const shared_this = shared_from_this();
  auto subdir = std::shared_ptr<dirtree_dir>(shared_this, this);
  std::istringstream pathtokens(path);
  std::string component;
  while (std::getline(pathtokens, component, '/'))
    if (!component.empty())
      subdir = subdir->get_subdir(component);
  return subdir;
}

void dirtree_dir::put_file(std::string const & path, std::shared_ptr<dirtree> child)
{
  auto sep = path.rfind('/');
  auto name = sep == path.npos ? path : path.substr(sep + 1);
  auto parent = sep == path.npos ? "/" : path.substr(0, sep);
  subdir_for_path(parent)->put_child(name, child);
}

auto get_child_entry(dirtree_dir * const dir, std::string const & name)
{
  return std::find_if(dir->entries.begin(), dir->entries.end(), [&](auto & entry) -> bool { return name == entry.name; });
}

std::shared_ptr<dirtree_dir> dirtree_dir::get_subdir(std::string const & name)
{
  auto entry = get_child_entry(this, name);
  if (entry != entries.end())
    {
      if (entry->inode->inode_type == SQFS_INODE_TYPE_DIR)
        return std::shared_ptr<dirtree_dir>(entry->inode, static_cast<dirtree_dir *>(entry->inode.get()));
      else
        {
          auto subdir = new dirtree_dir(wr);
          entry->inode = std::shared_ptr<dirtree>(subdir);
          return std::shared_ptr<dirtree_dir>(entry->inode, subdir);
        }
    }

  auto const subdir = new dirtree_dir(wr);
  std::shared_ptr<dirtree> subdir_shared(subdir);
  entries.push_back({name, subdir_shared});
  return std::shared_ptr<dirtree_dir>(subdir_shared, subdir);
}

void dirtree_dir::put_child(std::string const & name, std::shared_ptr<dirtree> child)
{
  auto entry = get_child_entry(this, name);
  if (entry == entries.end())
    entries.push_back({name, child});
  else
    entry->inode = child;
}
