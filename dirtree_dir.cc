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

std::shared_ptr<dirtree> dirtree_dir_new(sqsh_writer * wr)
{
  return std::shared_ptr<dirtree>(new dirtree_dir(wr));
}

static std::shared_ptr<dirtree> dirtree_get_child(sqsh_writer * wr, std::shared_ptr<dirtree> const dt, std::string const & name, std::shared_ptr<dirtree> (*con)(sqsh_writer *))
{
  dirtree_dir & dir = *static_cast<dirtree_dir *>(&*dt);
  auto entry = std::find_if(dir.entries.begin(), dir.entries.end(), [&](auto entry) -> bool { return name == entry.name; });
  if (entry != dir.entries.end())
    return entry->inode;

  auto child = con(wr);
  dir.entries.push_back({name, child});
  return child;
}

std::shared_ptr<dirtree> dirtree_get_subdir(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const dt, std::string const & name)
{
  return dirtree_get_child(wr, dt, name, dirtree_dir_new);
}

std::shared_ptr<dirtree> dirtree_put_reg(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const dt, std::string const & name)
{
  return dirtree_get_child(wr, dt, name, dirtree_reg_new);
}

std::shared_ptr<dirtree> dirtree_get_subdir_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const dt, std::string const & path)
{
  std::shared_ptr<dirtree> subdir = dt;
  std::istringstream pathtokens(path);
  std::string component;
  while (std::getline(pathtokens, component, '/'))
    if (!component.empty())
      subdir = dirtree_get_subdir(wr, subdir, component);
  return subdir;
}

static std::shared_ptr<dirtree> dirtree_put_nondir_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const root, std::string const & path, std::shared_ptr<dirtree> (*con)(struct sqsh_writer *))
{
  auto sep = path.rfind('/');
  auto name = sep == path.npos ? path : path.substr(sep + 1);
  auto parent = sep == path.npos ? "/" : path.substr(0, sep);

  std::shared_ptr<dirtree> parent_dt = dirtree_get_subdir_for_path(wr, root, parent);
  return parent_dt == nullptr ? nullptr : dirtree_get_child(wr, parent_dt, name, con);
}

std::shared_ptr<dirtree> dirtree_put_reg_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const root, std::string const & path)
{
  return dirtree_put_nondir_for_path(wr, root, path, dirtree_reg_new);
}

std::shared_ptr<dirtree> dirtree_put_sym_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const root, std::string const & path, std::string const & target)
{
  std::shared_ptr<dirtree> const sym = dirtree_put_nondir_for_path(wr, root, path, dirtree_sym_new);
  if (sym != nullptr)
    static_cast<dirtree_sym *>(&*sym)->target = target;

  return sym;
}

std::shared_ptr<dirtree> dirtree_put_dev_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const root, std::string const & path, uint16_t type, uint32_t rdev)
{
  std::shared_ptr<dirtree> const dev = dirtree_put_nondir_for_path(wr, root, path, dirtree_dev_new);
  if (dev != nullptr)
    {
      dev->inode_type = type;
      static_cast<dirtree_dev *>(&*dev)->rdev = rdev;
    }

  return dev;
}

std::shared_ptr<dirtree> dirtree_put_ipc_for_path(struct sqsh_writer * const wr, std::shared_ptr<dirtree> const root, std::string const & path, uint16_t type)
{
  std::shared_ptr<dirtree> const ipc = dirtree_put_nondir_for_path(wr, root, path, dirtree_ipc_new);
  if (ipc != nullptr)
    ipc->inode_type = type;

  return ipc;
}
