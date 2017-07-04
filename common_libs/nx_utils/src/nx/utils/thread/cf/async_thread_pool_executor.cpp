#include <algorithm>
#include "async_thread_pool_executor.h"

namespace cf {
async_thread_pool_executor::worker_thread::worker_thread() {
  thread_ = std::thread([this] {
    std::unique_lock<std::mutex> lock(m_);
    while (!need_stop_) {
      start_cond_.wait(lock, [this] {
        return (bool)task_ || need_stop_;
      });
      if (need_stop_)
        return;
      lock.unlock();
      task_();
      lock.lock();
      task_ = nullptr;
      has_task_ = false;
      completion_cb_();
    }
  });
}

async_thread_pool_executor::worker_thread::~worker_thread() {
  stop();
}

void async_thread_pool_executor::worker_thread::stop() {
  {
    std::lock_guard<std::mutex> lock(m_);
    need_stop_ = true;
    start_cond_.notify_all();
  }
  if (thread_.joinable())
    thread_.join();
}

bool async_thread_pool_executor::worker_thread::available() const {
  return !has_task_;
}

void async_thread_pool_executor::worker_thread::post(
    const detail::task_type& task,
    const detail::task_type& completion_cb) {
  std::unique_lock<std::mutex> lock(m_);
  if (task_)
    throw std::logic_error("Worker already has a pending task");
  start_task(task, completion_cb);
}

void async_thread_pool_executor::worker_thread::start_task(
    const detail::task_type& task,
    const detail::task_type& completion_cb) {
  has_task_ = true;
  task_ = task;
  completion_cb_ = completion_cb;
  start_cond_.notify_one();
}

async_thread_pool_executor::async_thread_pool_executor(size_t size)
  : tp_(size),
    available_count_(size) {
  manager_thread_ = std::thread([this] {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!need_stop_) {
      cond_.wait(lock, [this] {
        return need_stop_ || (!task_queue_.empty() &&
                              available_count_ > 0);
      });
      while (!task_queue_.empty() && available_count_ > 0) {
        if (need_stop_)
          return;
        auto ready_it = std::find_if(tp_.begin(), tp_.end(),
        [](const worker_thread& worker) {
            return worker.available();
        });
        if (ready_it == tp_.end())
          break;
        auto task = task_queue_.front();
        task_queue_.pop();
        --available_count_;
        ready_it->post(task, [this] {
          ++available_count_;
          cond_.notify_one();
        });
      }
    }
  });
}

async_thread_pool_executor::~async_thread_pool_executor() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    need_stop_ = true;
    cond_.notify_all();
  }
  if (manager_thread_.joinable())
    manager_thread_.join();
  for (auto& worker : tp_) {
    worker.stop();
  }
}

size_t async_thread_pool_executor::available() const {
  return (size_t)available_count_;
}

void async_thread_pool_executor::post(const detail::task_type& task) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    task_queue_.push(task);
  }
  cond_.notify_one();
}
}