// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

namespace cf {
namespace detail {

// We can't use std::function as continuation holder, as it
// requires held type to be copyable. So here is simple move-only
// callable wrapper.
template<typename F>
class movable_func;

template<typename R, typename... Args>
class movable_func<R(Args...)> {

  struct base_holder {
    virtual R operator() (Args... args) = 0;
    virtual ~base_holder() {}
  };

  template<typename F>
  struct holder : base_holder {
    holder(F f) : f_(std::move(f)) {}
    virtual R operator() (Args... args) override {
      return f_(args...);
    }

    F f_;
  };

public:
  template<typename F>
  movable_func(F f) : held_(new holder<F>(std::move(f))) {}
  movable_func(std::nullptr_t): held_(nullptr) {}
  movable_func() : held_(nullptr) {}
  movable_func(movable_func<R(Args...)>&&) = default;
  movable_func& operator = (movable_func<R(Args...)>&&) = default;
  movable_func(const movable_func<R(Args...)>&) = delete;
  movable_func& operator = (const movable_func<R(Args...)>&) = delete;
  bool empty() const { return !held_; }

  explicit operator bool() const { return !empty(); }

  R operator() (Args... args) const {
    return held_->operator()(args...);
  }

private:
  std::unique_ptr<base_holder> held_;
};

using task_type = movable_func<void()>;
}
}
