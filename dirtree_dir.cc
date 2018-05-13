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

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "dirtree.h"
#include "sqsh_defs.h"
#include "sqsh_writer.h"

dirtree_dir & dirtree_dir::subdir_for_path(std::string const & path)
{
  auto subdir = this;
  std::istringstream pathtokens(path);
  std::string component;
  while (std::getline(pathtokens, component, '/'))
    if (!component.empty())
      subdir = &subdir->get_subdir(component);
  return *subdir;
}

dirtree & dirtree_dir::put_file(std::string const & path,
                                std::unique_ptr<dirtree> && child)
{
  auto sep = path.rfind('/');
  auto name = sep == path.npos ? path : path.substr(sep + 1);
  auto parent = sep == path.npos ? "/" : path.substr(0, sep);
  return subdir_for_path(parent).put_child(name, std::move(child));
}

dirtree_dir & dirtree_dir::get_subdir(std::string const & name)
{
  auto entry = entries.find(name);
  if (entry != entries.end() &&
      entry->second->inode_type == SQFS_INODE_TYPE_DIR)
    return *static_cast<dirtree_dir *>(entry->second.get());

  entries[name] = std::make_unique<dirtree_dir>(wr);
  return *static_cast<dirtree_dir *>(entries[name].get());
}
