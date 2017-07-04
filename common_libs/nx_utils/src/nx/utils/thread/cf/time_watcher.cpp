#include <iostream>
#include "time_watcher.h"

namespace cf {
time_watcher::time_watcher()
: wakeup_time_(year_forward()){
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

time_watcher::~time_watcher() {
  need_stop_ = true;
  cond_.notify_one();
  if (watcher_thread_.joinable())
    watcher_thread_.join();
}

bool time_watcher::time_has_come() const {
  if (record_set_.empty())
    return false;
  return record_set_.cbegin()->time <= std::chrono::steady_clock::now();
}

std::chrono::time_point<std::chrono::steady_clock> time_watcher::year_forward() {
  return std::chrono::steady_clock::now() + std::chrono::hours(365 * 24);
}

}
