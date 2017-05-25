#include "async_queued_executor.h"

namespace cf {
async_queued_executor::async_queued_executor() : need_stop_(false) {
  start();
}

async_queued_executor::~async_queued_executor() {
  stop();
}

void async_queued_executor::post(const detail::task_type& f) {
  std::lock_guard<std::mutex> lock(mutex_);
  tasks_.push(f);
  cond_.notify_all();
}

void async_queued_executor::stop() {
  {
    std::lock_guard<std::mutex> lock_(mutex_);
    need_stop_ = true;
    cond_.notify_all();
  }
  if (thread_.joinable())
    thread_.join();
}

void async_queued_executor::start() {
  thread_ = std::thread([this] {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!need_stop_) {
      cond_.wait(lock, [this] { return need_stop_ || !tasks_.empty(); });
      if (need_stop_)
        break;
      while (!tasks_.empty()) {
        auto f = tasks_.front();
        tasks_.pop();
        lock.unlock();
        f();
        lock.lock();
      }
    }
  });
}
}