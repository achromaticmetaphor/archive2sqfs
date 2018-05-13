/*
Copyright (C) 2017, 2018  Charles Cagle

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

#ifndef LSL_ARCHIVE_READER_H
#define LSL_ARCHIVE_READER_H

#include <cstddef>
#include <stdexcept>
#include <string>

using namespace std::literals;

#include <archive.h>
#include <archive_entry.h>

class archive_reader
{
public:
  struct archive_entry * entry;

  archive_reader(char const *);
  template <typename S> archive_reader(S);
  archive_reader(std::FILE *);
  ~archive_reader();

  bool next();
  void read(void *, std::size_t);
  template <typename T> void read(T &, std::size_t);

private:
  struct archive * reader;

  archive_reader();
};

static std::size_t const reader_blocksize = 10240;

archive_reader::archive_reader()
{
  reader = archive_read_new();
  if (reader == nullptr)
    throw std::runtime_error("failed to initialize reader object"s);
  if (archive_read_support_filter_all(reader) != ARCHIVE_OK)
    throw std::runtime_error("failed to enable all input filters"s);
  if (archive_read_support_format_all(reader) != ARCHIVE_OK)
    throw std::runtime_error("failed to enable all input formats"s);
}

archive_reader::archive_reader(char const * const pathname) : archive_reader()
{
  if (archive_read_open_filename(reader, pathname, reader_blocksize) !=
      ARCHIVE_OK)
    throw std::runtime_error("failed to open named archive"s);
}

template <typename S>
archive_reader::archive_reader(S pathname) : archive_reader(pathname.data())
{
}

archive_reader::archive_reader(std::FILE * const handle) : archive_reader()
{
  if (archive_read_open_FILE(reader, handle) != ARCHIVE_OK)
    throw std::runtime_error("failed to open archive by handle"s);
}

archive_reader::~archive_reader()
{
  if (reader != nullptr)
    archive_read_free(reader);
}

bool archive_reader::next()
{
  auto const result = archive_read_next_header(reader, &entry);
  if (result == ARCHIVE_FATAL)
    throw std::runtime_error("fatal error in archive_read_next_header()"s);
  return result == ARCHIVE_OK;
}

void archive_reader::read(void * const buff, std::size_t const len)
{
  if (archive_read_data(reader, buff, len) != len)
    throw std::runtime_error("failed to read data from archive"s);
}

template <typename T>
void archive_reader::read(T & con, std::size_t const len)
{
  con.resize(len);
  read(con.data(), len);
}

#endif
