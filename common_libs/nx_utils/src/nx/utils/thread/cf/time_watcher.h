#pragma once

#include <chrono>
#include <set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include "common.h"

namespace cf {
class time_watcher {
  struct record {
    std::chrono::time_point<std::chrono::steady_clock> time;
    detail::task_type task;
    record(std::chrono::milliseconds timeout,
           const detail::task_type& task)
    : time(std::chrono::steady_clock::now() + timeout),
      task(task) {}
  };
  
  friend bool operator < (const record& lhs, const record& rhs) {
    return lhs.time < rhs.time;
  }
  
public:
  time_watcher();
  ~time_watcher();
  
  template<typename Rep, typename Period>
  void add(const detail::task_type& task,
           const std::chrono::duration<Rep, Period>& timeout) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      record_set_.emplace(
          std::chrono::duration_cast<std::chrono::milliseconds>(timeout),
          task);
    }
    cond_.notify_one();
  }
  
  private:
    bool time_has_come() const;
  
private:
  std::condition_variable cond_;
  std::mutex mutex_;
  std::set<record> record_set_;
  std::thread watcher_thread_;
  std::atomic<bool> need_stop_ = {false};
  std::chrono::time_point<std::chrono::steady_clock> wakeup_time_;
};
}
