#pragma once

#include <atomic>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <nx/utils/std/thread.h>

#include "common.h"

namespace cf {
class async_thread_pool_executor {
  class worker_thread {
  public:
    worker_thread();
    ~worker_thread();

    void stop();
    bool available() const;
    
    void post(const detail::task_type& task,
              const detail::task_type& completion_cb);

  private:
    void start_task(const detail::task_type& task,
                    const detail::task_type& completion_cb);

  private:
    nx::utils::thread thread_;
    mutable std::mutex m_;
    std::atomic<bool> need_stop_ = {false};
    std::atomic<bool> has_task_ = {false};
    std::condition_variable start_cond_;
    detail::task_type task_;
    detail::task_type completion_cb_;
  };

  using tp_type = std::vector<worker_thread>;

public:
  async_thread_pool_executor(size_t size);
  ~async_thread_pool_executor();

  size_t available() const;
  void post(const detail::task_type& task);

private:
  tp_type tp_;
  mutable std::mutex mutex_;
  std::queue<detail::task_type> task_queue_;
  nx::utils::thread manager_thread_;
  std::atomic<bool> need_stop_ = {false};
  std::atomic<size_t> available_count_;
  std::condition_variable cond_;
};
}
