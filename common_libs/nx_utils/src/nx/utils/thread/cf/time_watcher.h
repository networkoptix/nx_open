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
  time_watcher()
    : wakeup_time_(year_forward()) {
    watcher_thread_ = std::thread([this] {
      while (!need_stop_) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!record_set_.empty())
          wakeup_time_ = record_set_.begin()->time;
        while (!need_stop_ && !time_has_come()) {
          cond_.wait_until(lock, wakeup_time_);
          if (!time_has_come() && !record_set_.empty()) {
            wakeup_time_ = record_set_.begin()->time;
          }
        }
        if (need_stop_)
          return;
        while (time_has_come()) {
          record_set_.begin()->task();
          record_set_.erase(record_set_.begin());
        }
        wakeup_time_ = record_set_.empty() ?
          year_forward() :
          record_set_.begin()->time;
      }
    });
  }

  ~time_watcher() {
    need_stop_ = true;
    cond_.notify_one();
    if (watcher_thread_.joinable())
      watcher_thread_.join();
  }

  void add(const detail::task_type& task, std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    record_set_.emplace(timeout, task);
    cond_.notify_one();
  }

private:
  bool time_has_come() const {
    if (record_set_.empty())
      return false;
    return record_set_.cbegin()->time <= std::chrono::steady_clock::now();
  }

  std::chrono::time_point<std::chrono::steady_clock> year_forward() {
    return std::chrono::steady_clock::now() + std::chrono::hours(365 * 24);
  }

private:
  std::condition_variable cond_;
  std::mutex mutex_;
  std::set<record> record_set_;
  std::thread watcher_thread_;
  std::atomic<bool> need_stop_ = {false};
  std::chrono::time_point<std::chrono::steady_clock> wakeup_time_;
};
}
