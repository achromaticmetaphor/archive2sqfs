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

#ifndef LSL_BOUNDED_WORK_QUEUE_H
#define LSL_BOUNDED_WORK_QUEUE_H

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

#include "optional.h"

template <typename T> class bounded_work_queue
{
  bool finished;
  std::size_t bound;
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable popped;
  std::condition_variable pushed;

public:
  bounded_work_queue(std::size_t bound) : finished(false), bound(bound) {}

  void push(T && e)
  {
    std::unique_lock<decltype(mutex)> lock(mutex);
    popped.wait(lock, [&]() { return queue.size() < bound; });
    queue.push(std::move(e));
    pushed.notify_all();
  }

  optional<T> pop()
  {
    std::unique_lock<decltype(mutex)> lock(mutex);
    pushed.wait(lock, [&]() { return finished || !queue.empty(); });
    if (queue.empty())
      return {};
    auto e = std::move(queue.front());
    queue.pop();
    popped.notify_all();
    return std::move(e);
  }

  void finish()
  {
    std::lock_guard<decltype(mutex)> guard(mutex);
    finished = true;
    pushed.notify_all();
  }
};

#endif
