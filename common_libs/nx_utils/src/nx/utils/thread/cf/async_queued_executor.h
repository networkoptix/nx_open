#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <nx/utils/std/thread.h>

#include "common.h"

namespace cf {
class async_queued_executor {
public:
  async_queued_executor();
  ~async_queued_executor();

  void post(const detail::task_type& f);
  void stop();

private:
  void start();

private:
  std::queue<detail::task_type> tasks_;
  std::mutex mutex_;
  std::condition_variable cond_;
  nx::utils::thread thread_;
  bool need_stop_;
};
}
