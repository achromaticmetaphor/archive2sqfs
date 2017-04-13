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

template <typename C>
static dirtree * dirtree_get_child(dirtree_dir * const dir, std::string const & name, C con)
{
  auto entry = std::find_if(dir->entries.begin(), dir->entries.end(), [&](auto & entry) -> bool { return name == entry.name; });
  if (entry != dir->entries.end())
    return &*entry->inode;

  auto child = con();
  dir->entries.push_back({name, std::move(child)});
  return &*dir->entries.back().inode;
}

dirtree_dir * dirtree_get_subdir(dirtree_dir * const dt, std::string const & name)
{
  return static_cast<dirtree_dir *>(dirtree_get_child(dt, name, [&]() -> auto { return std::unique_ptr<dirtree>(new dirtree_dir(dt->wr)); }));
}

dirtree_dir * dirtree_dir::subdir_for_path(std::string const & path)
{
  dirtree_dir * subdir = this;
  std::istringstream pathtokens(path);
  std::string component;
  while (std::getline(pathtokens, component, '/'))
    if (!component.empty())
      subdir = dirtree_get_subdir(subdir, component);
  return subdir;
}

template <typename C>
static dirtree * dirtree_put_nondir_for_path(dirtree_dir * const root, std::string const & path, C con)
{
  auto sep = path.rfind('/');
  auto name = sep == path.npos ? path : path.substr(sep + 1);
  auto parent = sep == path.npos ? "/" : path.substr(0, sep);

  auto parent_dt = root->subdir_for_path(parent);
  return parent_dt == nullptr ? nullptr : dirtree_get_child(static_cast<dirtree_dir *>(&*parent_dt), name, con);
}

dirtree_reg * dirtree_dir::put_reg(std::string const & path)
{
  return static_cast<dirtree_reg *>(dirtree_put_nondir_for_path(this, path, [&]() -> auto { return std::unique_ptr<dirtree>(new dirtree_reg(wr)); }));
}

dirtree_sym * dirtree_dir::put_sym(std::string const & path, std::string const & target)
{
  return static_cast<dirtree_sym *>(dirtree_put_nondir_for_path(this, path, [&]() -> auto { return std::unique_ptr<dirtree>(new dirtree_sym(wr, target)); }));
}

dirtree_dev * dirtree_dir::put_dev(std::string const & path, uint16_t type, uint32_t rdev)
{
  return static_cast<dirtree_dev *>(dirtree_put_nondir_for_path(this, path, [&]() -> auto { return std::unique_ptr<dirtree>(new dirtree_dev(wr, type, rdev)); }));
}

dirtree_ipc * dirtree_dir::put_ipc(std::string const & path, uint16_t type)
{
  return static_cast<dirtree_ipc *>(dirtree_put_nondir_for_path(this, path, [&]() -> auto { return std::unique_ptr<dirtree>(new dirtree_ipc(wr, type)); }));
}
