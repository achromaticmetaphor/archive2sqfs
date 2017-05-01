/*
Copyright (C) 2017  Charles Cagle

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

#include <archive.h>
#include <archive_entry.h>

struct archive_reader
{
  struct archive * reader;
  struct archive_entry * entry;

  archive_reader()
  {
    reader = archive_read_new();
    if (reader == nullptr || archive_read_support_filter_all(reader) != ARCHIVE_OK || archive_read_support_format_all(reader) != ARCHIVE_OK)
      throw "failed to allocate or initialize archive_reader";
  }

  archive_reader(char const * const pathname) : archive_reader()
  {
    if (archive_read_open_filename(reader, pathname, 10240) != ARCHIVE_OK)
      throw "failed to open archive";
  }

  archive_reader(std::FILE * const handle) : archive_reader()
  {
    if (archive_read_open_FILE(reader, handle) != ARCHIVE_OK)
      throw "failed to open archive";
  }

  ~archive_reader()
  {
    if (reader != nullptr)
      archive_read_free(reader);
  }

  bool next()
  {
    auto const result = archive_read_next_header(reader, &entry);
    if (result == ARCHIVE_FATAL)
      throw "fatal libarchive error";
    return result == ARCHIVE_OK;
  }

  auto read(void * const buff, size_t const len)
  {
    return archive_read_data(reader, buff, len);
  }
};
