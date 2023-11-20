// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include "common.h"

#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 8)
    #include "cpp14_type_traits.h"
#endif

namespace cf {

// Future<void> and promise<void> are explicitly forbidden.
// Use cf::unit instead.
struct unit {};
inline bool operator == (unit, unit) { return true; }
inline bool operator != (unit, unit) { return false; }

enum class future_status {
  ready,
  timeout
};

enum class timeout_state {
  not_set,
  expired,
  result_set
};

#define ERRC_LIST(APPLY) \
  APPLY(broken_promise) \
  APPLY(future_already_retrieved) \
  APPLY(promise_already_satisfied) \
  APPLY(no_state)

#define ENUM_APPLY(value) value,

enum class errc {
  ERRC_LIST(ENUM_APPLY)
};

#define STRING_APPLY(value) case errc::value: return #value;

inline std::string errc_string(errc value) {
  switch (value) {
    ERRC_LIST(STRING_APPLY)
  };
  return "";
}

#undef ENUM_APPLY
#undef STRING_APPLY
#undef ERRC_LIST

struct future_error : public std::exception {
  future_error(errc ecode, const std::string& s)
    : ecode_(ecode),
      error_string_(s) {}

  virtual const char* what() const noexcept override {
    return error_string_.data();
  }

  errc ecode() const {
    return ecode_;
  }

  errc ecode_;
  std::string error_string_;
};

template<typename T>
class future;

template<typename T>
future<T> make_ready_future(T&& t);

namespace detail {

template<typename Derived>
class shared_state_base {
  using cb_type = movable_func<void()>;
public:
  ~shared_state_base() {}
  shared_state_base()
    : satisfied_(false),
      executed_(false)
  {}

  void wait() const {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return satisfied_ == true; });
  }

  template<typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) const {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait_for(lock, timeout, [this] { return satisfied_ == true; });
    return satisfied_ ? future_status::ready : future_status::timeout;
  }

  template<typename Clock, typename Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& timepoint) const {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait_until(lock, timepoint, [this] { return satisfied_ == true; });
    return satisfied_ ? future_status::ready : future_status::timeout;
  }

  void set_ready(std::unique_lock<std::mutex>& lock) {
    satisfied_ = true;
    cond_.notify_all();
    if (cb_.empty()) {
      return;
    }
    bool need_execute = !executed_;
    if (!need_execute) {
      return;
    }
    executed_ = true;
    lock.unlock();
    if (need_execute) {
      cb_();
    }
  }

  template<typename F>
  void set_callback(F&& f) {
    std::unique_lock<std::mutex> lock(mutex_);
    cb_ = std::forward<F>(f);
    bool need_execute = satisfied_ && !executed_;
    if (!need_execute)
      return;
    executed_ = true;
    lock.unlock();
    if (need_execute) {
      cb_();
    }
  }

  bool is_ready() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return satisfied_;
  }

  void set_exception(std::exception_ptr p) {
    std::unique_lock<std::mutex> lock(mutex_);
    throw_if_satisfied();
    exception_ptr_ = p;
    set_ready(lock);
  }

  bool has_exception() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (bool)exception_ptr_;
  }

  std::exception_ptr get_exception() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exception_ptr_;
  }

  void abandon() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (satisfied_)
      return;
    exception_ptr_ = std::make_exception_ptr(
        future_error(errc::broken_promise,
                     errc_string(errc::broken_promise)));
    set_ready(lock);
  }

  std::mutex& get_timeout_mutex() { return timeout_mutex_; }

  timeout_state expired() const { return timeout_state_; }
  void expired(timeout_state state) {timeout_state_ = state; }

protected:
  void throw_if_satisfied() {
    if (satisfied_)
      throw future_error(errc::promise_already_satisfied,
                         errc_string(errc::promise_already_satisfied));
  }

protected:
  mutable std::mutex mutex_;
  mutable std::condition_variable cond_;
  bool satisfied_;
  bool executed_;
  std::exception_ptr exception_ptr_;
  cb_type cb_;
  timeout_state timeout_state_ = timeout_state::not_set;
  std::mutex timeout_mutex_;
};

template<typename T>
class shared_state : public shared_state_base<shared_state<T>>,
                     public std::enable_shared_from_this<shared_state<T>> {
  using value_type = T;
  using base_type = shared_state_base<shared_state<T>>;

public:
  template<typename U>
  void set_value(U&& value) {
    std::unique_lock<std::mutex> lock(base_type::mutex_);
    base_type::throw_if_satisfied();
    value_ = std::forward<U>(value);
    base_type::set_ready(lock);
  }

  value_type get_value() {
    base_type::wait();
    if (base_type::exception_ptr_)
      std::rethrow_exception(base_type::exception_ptr_);
    return std::move(value_);
  }

private:
  value_type value_;
};

template<typename T>
using shared_state_ptr = std::shared_ptr<shared_state<T>>;

template<typename T>
void check_state(const shared_state_ptr<T>& state) {
  if (!state)
    throw future_error(errc::no_state, errc_string(errc::no_state));
}

// various type helpers

// get the return type of a continuation callable
template<typename T, typename F>
using then_arg_ret_type = std::invoke_result_t<std::decay_t<F>, future<T>>;

template<typename F, typename... Args>
using callable_ret_type = std::invoke_result_t<std::decay_t<F>, Args...>;

template<typename T>
struct is_future {
  const static bool value = false;
};

template<typename T>
struct is_future<future<T>> {
  const static bool value = true;
};

// future<T>::then(F&& f) return type
template<typename T, typename F>
using then_ret_type = std::conditional_t<
  is_future<then_arg_ret_type<T, F>>::value,  // if f returns future<U>
  then_arg_ret_type<T, F>,                    // then leave type untouched
  future<then_arg_ret_type<T, F>> >;          // else lift it into the future type

template<typename T>
struct future_held_type;

template<typename T>
struct future_held_type<future<T>> {
  using type = std::decay_t<T>;
};

template<typename R>
void arg_type(R(*)());

template<typename R, typename A>
A arg_type(R(*)(A));

template<typename R, typename C>
void arg_type(R(C::*)());

template<typename R, typename C, typename A>
A arg_type(R(C::*)(A));

template<typename R, typename C>
void arg_type(R(C::*)() const);

template<typename R, typename C, typename A>
A arg_type(R(C::*)(A) const);

template<typename F>
decltype(arg_type(&F::operator())) arg_type(F f);

template<typename F>
using arg_type_t = decltype(arg_type(std::declval<F>()));

template<typename F>
auto make_then_unwrap_handler(F&& f) {
  return [f = std::forward<F>(f)](auto future) mutable {
    return std::move(f)(future.get());
  };
}

template<typename T, typename U>
auto ensure_future(U&& u) {
  return make_ready_future<T>(std::forward<U>(u));
}

template<typename T>
auto ensure_future(future<T> t) {
    return t;
}

template<typename F>
auto make_catch_handler(F&& f) {
  return [f = std::forward<F>(f)](auto future) mutable {
    using T = decltype(future.get());
    using E = arg_type_t<std::decay_t<F>>;
    try {
      return make_ready_future<T>(future.get());
    } catch (E e) {
      return ensure_future<T>(std::move(f)(std::forward<E>(e)));
    }
  };
}

} // namespace detail

template<typename T>
class future {
  template<typename U>
  friend class promise;

  template<typename U>
  friend future<U> make_ready_future(U&& u);

  template<typename U>
  friend future<U> make_exceptional_future(std::exception_ptr p);

public:
  using value_type = T;

  future() = default;

  future(const future<T>& other) = delete;
  future<T>& operator = (const future<T>& other) = delete;

  future(future<T>&& other)
    : state_(std::move(other.state_)) {}

  future<T>& operator = (future<T>&& other) {
    state_ = std::move(other.state_);
    return *this;
  }

  bool valid() const {
    return state_ != nullptr;
  }

  T get() {
    check_state(state_);
    return state_->get_value();
  }

  std::exception_ptr exception() const {
      check_state(state_);
      return state_->get_exception();
  }

  template<typename F>
  detail::then_ret_type<T, F> then(F&& f);

  template<typename F>
  auto then_unwrap(F&& f);

  template<typename F>
  auto catch_(F&& f);

template<typename Rep, typename Period, typename TimeWatcher, typename Exception>
future<T> timeout(std::chrono::duration<Rep, Period> duration,
                  const Exception& exception,
                  TimeWatcher& watcher);

  template<typename F, typename Executor>
  detail::then_ret_type<T, F> then(Executor& executor, F&& f);

  template<typename F, typename Executor>
  auto then_unwrap(Executor& executor, F&& f);

  template<typename F, typename Executor>
  auto catch_(Executor& executor, F&& f);

  bool is_ready() const {
    check_state(state_);
    return state_->is_ready();
  }

  void wait() const {
    check_state(state_);
    if (state_)
      state_->wait();
  }

  template<typename Rep, typename Period>
  cf::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) {
    check_state(state_);
    return state_->wait_for(timeout);
  }

  template<typename Clock, typename Duration>
  cf::future_status wait_until(const std::chrono::time_point<Clock, Duration>& timepoint) {
    check_state(state_);
    return state_->wait_until(timepoint);
  }

private:
  future(const detail::shared_state_ptr<T>& state)
    : state_(state) {}

  template<typename F>
  typename std::enable_if<
    detail::is_future<
      detail::then_arg_ret_type<T, F>
    >::value,
    detail::then_ret_type<T, F>
  >::type
  then_impl(F&& f);

  template<typename F>
  typename std::enable_if<
    !detail::is_future<
      detail::then_arg_ret_type<T, F>
    >::value,
    detail::then_ret_type<T, F>
  >::type
  then_impl(F&& f);

  template<typename F, typename Executor>
  typename std::enable_if<
    detail::is_future<
      detail::then_arg_ret_type<T, F>
    >::value,
    detail::then_ret_type<T, F>
  >::type
  then_impl(F&& f, Executor& executor);

  template<typename F, typename Executor>
  typename std::enable_if<
    !detail::is_future<
      detail::then_arg_ret_type<T, F>
    >::value,
    detail::then_ret_type<T, F>
  >::type
  then_impl(F&& f, Executor& executor);

  template<typename F>
  void set_callback(F&& f) {
    check_state(state_);
    state_->set_callback(std::forward<F>(f));
  }

private:
  detail::shared_state_ptr<T> state_;
};

template<typename T>
future<T> make_exceptional_future(std::exception_ptr p);

// TODO: shared_future
// TODO: T& specialization. Forbid and test.

template<>
class future<void>;

template<typename T>
class future<T&>;

template<typename T>
template<typename F>
detail::then_ret_type<T, F> future<T>::then(F&& f) {
  check_state(state_);
  return then_impl<F>(std::forward<F>(f));
}

template<typename T>
template<typename F>
auto future<T>::then_unwrap(F&& f) {
  return then(detail::make_then_unwrap_handler(std::forward<F>(f)));
}

template<typename T>
template<typename F>
auto future<T>::catch_(F&& f) {
  return then(detail::make_catch_handler(std::forward<F>(f)));
}

template<typename T>
template<typename F, typename Executor>
detail::then_ret_type<T, F> future<T>::then(Executor& executor, F&& f) {
  check_state(state_);
  return then_impl<F>(std::forward<F>(f), executor);
}

template<typename T>
template<typename F, typename Executor>
auto future<T>::then_unwrap(Executor& executor, F&& f) {
  return then(executor, detail::make_then_unwrap_handler(std::forward<F>(f)));
}

template<typename T>
template<typename F, typename Executor>
auto future<T>::catch_(Executor& executor, F&& f) {
  return then(executor, detail::make_catch_handler(std::forward<F>(f)));
}

template<typename T>
class promise;

// future<R> F(future<T>) specialization
template<typename T>
template<typename F>
typename std::enable_if<
  detail::is_future<
    detail::then_arg_ret_type<T, F>
  >::value,
  detail::then_ret_type<T, F>
>::type
future<T>::then_impl(F&& f) {
  using R = typename detail::future_held_type<
    detail::then_arg_ret_type<T, F>
  >::type;
  using S = typename std::remove_reference<decltype(*this->state_)>::type;
  promise<R> p;
  future<R> ret = p.get_future();
  set_callback([p = std::move(p), f = std::forward<F>(f),
               state = std::weak_ptr<S>(this->state_->shared_from_this())] () mutable {
    auto sp_state = state.lock();
    cf::future<T> arg_future;

    if (sp_state->has_exception()) {
      arg_future = cf::make_exceptional_future<T>(sp_state->get_exception());
    } else {
      arg_future = cf::make_ready_future<T>(sp_state->get_value());
    }

    try {
      auto inner_f = f(std::move(arg_future));
      inner_f.then([p = std::move(p)] (cf::future<R> f) mutable {
        try {
          p.set_value(f.get());
        } catch (...) {
          p.set_exception(std::current_exception());
        }
        return cf::unit();
      });
    } catch (...) {
      p.set_exception(std::current_exception());
    }

  });

  return ret;
}

// R F(future<T>) specialization
template<typename T>
template<typename F>
typename std::enable_if<
  !detail::is_future<
    detail::then_arg_ret_type<T, F>
  >::value,
  detail::then_ret_type<T, F>
>::type
future<T>::then_impl(F&& f) {
  using R = detail::then_arg_ret_type<T, F>;
  using S = typename std::remove_reference<decltype(*this->state_)>::type;
  promise<R> p;
  future<R> ret = p.get_future();
  set_callback([p = std::move(p), f = std::forward<F>(f),
               state = std::weak_ptr<S>(this->state_->shared_from_this())] () mutable {
    auto sp_state = state.lock();
    cf::future<T> arg_future;

    if (sp_state->has_exception()) {
      arg_future = cf::make_exceptional_future<T>(sp_state->get_exception());
    } else {
      arg_future = cf::make_ready_future<T>(sp_state->get_value());
    }

    try {
      auto&& result = f(std::move(arg_future));
      p.set_value(std::move(result));
    } catch (...) {
      p.set_exception(std::current_exception());
    }
  });

  return ret;
}

// future<R> F(future<T>) specialization via executor
template<typename T>
template<typename F, typename Executor>
typename std::enable_if<
  detail::is_future<
    detail::then_arg_ret_type<T, F>
  >::value,
  detail::then_ret_type<T, F>
>::type
future<T>::then_impl(F&& f, Executor& executor) {
  using R = typename detail::future_held_type<
    detail::then_arg_ret_type<T, F>
  >::type;
  using S = typename std::remove_reference<decltype(*this->state_)>::type;
  promise<R> p;
  future<R> ret = p.get_future();
  set_callback([p = std::move(p), f = std::forward<F>(f),
               state = std::weak_ptr<S>(this->state_->shared_from_this()), &executor] () mutable {
    auto sp_state = state.lock();
    cf::future<T> arg_future;

    if (sp_state->has_exception()) {
      arg_future = cf::make_exceptional_future<T>(sp_state->get_exception());
    } else {
      arg_future = cf::make_ready_future<T>(sp_state->get_value());
    }

    auto promise_ptr = std::make_shared<promise<R>>(std::move(p));
    auto arg_future_ptr = std::make_shared<future<T>>(std::move(arg_future));

    executor.post([promise_ptr, arg_future_ptr, f = std::forward<F>(f), &executor] () mutable {
      try {
        auto inner_f = f(std::move(*arg_future_ptr));
        inner_f.then(executor, [promise_ptr] (cf::future<R> f) mutable {
          try {
            promise_ptr->set_value(f.get());
          } catch (...) {
            promise_ptr->set_exception(std::current_exception());
          }
          return cf::unit();
        });
      } catch (...) {
        promise_ptr->set_exception(std::current_exception());
      }
    });
  });

  return ret;
}

// R F(future<T>) specialization via executor
template<typename T>
template<typename F, typename Executor>
typename std::enable_if<
  !detail::is_future<
    detail::then_arg_ret_type<T, F>
  >::value,
  detail::then_ret_type<T, F>
>::type
future<T>::then_impl(F&& f, Executor& executor) {
  using R = detail::then_arg_ret_type<T, F>;
  using S = typename std::remove_reference<decltype(*this->state_)>::type;
  promise<R> p;
  future<R> ret = p.get_future();
  set_callback([p = std::move(p), f = std::forward<F>(f),
               state = std::weak_ptr<S>(this->state_->shared_from_this()), &executor] () mutable {
    auto sp_state = state.lock();
    cf::future<T> arg_future;

    if (sp_state->has_exception()) {
      arg_future = cf::make_exceptional_future<T>(sp_state->get_exception());
    } else {
      arg_future = cf::make_ready_future<T>(sp_state->get_value());
    }

    struct local_state {
      promise<R> p;
      F f;
      local_state(promise<R> p, F f)
        : p(std::move(p)),
          f(std::move(f)) {}
    };

    auto lstate = std::make_shared<local_state>(std::move(p), std::move(f));
    auto arg_future_ptr = std::make_shared<future<T>>(std::move(arg_future));

    executor.post([arg_future_ptr, lstate, sp_state] () mutable {
      try {
        auto&& result = lstate->f(std::move(*arg_future_ptr));
        lstate->p.set_value(std::move(result));
      } catch (...) {
        lstate->p.set_exception(std::current_exception());
      }
    });
  });

  return ret;
}

template<typename T>
template<typename Rep, typename Period, typename TimeWatcher, typename Exception>
future<T> future<T>::timeout(std::chrono::duration<Rep, Period> duration,
                             const Exception& exception,
                             TimeWatcher& watcher) {
  auto promise_ptr = std::make_shared<promise<T>>();
  future<T> ret = promise_ptr->get_future();

  watcher.add([promise_ptr,
               state = this->state_->shared_from_this(),
               exception] () mutable {
    std::lock_guard<std::mutex> lock(state->get_timeout_mutex());
    if (state->expired() == timeout_state::result_set)
      return;
    state->expired(timeout_state::expired);
    promise_ptr->set_exception(std::make_exception_ptr(exception));
  }, duration);

  set_callback([promise_ptr, state = this->state_->shared_from_this()] () mutable {
    std::lock_guard<std::mutex> lock(state->get_timeout_mutex());
    if (state->expired() == timeout_state::expired)
      return;
    state->expired(timeout_state::result_set);
    if (state->has_exception())
      promise_ptr->set_exception(state->get_exception());
    else {
      promise_ptr->set_value(state->get_value());
    }
  });

  return ret;
}

template<typename T>
class promise {
public:
  promise()
    : state_(std::make_shared<detail::shared_state<T>>()) {}

  promise(promise&& other)
    : state_(std::move(other.state_)) {}

  promise& operator = (promise&& other) {
    state_ = std::move(other.state_);
    return *this;
  }

  ~promise() {
    if (state_)
      state_->abandon();
  }

  void swap(promise& other) noexcept {
    state_.swap(other.state_);
  }

  template<typename U>
  void set_value(U&& value) {
    check_state(state_);
    state_->set_value(std::forward<U>(value));
  }

  future<T> get_future() {
    check_state(state_);
    if (state_.use_count() > 1) {
      throw future_error(errc::future_already_retrieved,
                         errc_string(errc::future_already_retrieved));
    }
    return future<T>(state_);
  }

  void set_exception(std::exception_ptr p) {
    check_state(state_);
    state_->set_exception(p);
  }

private:
  detail::shared_state_ptr<T> state_;
};

template<>
class promise<void>;

template<typename T>
class promise<T&>;

template<typename U>
future<U> make_ready_future(U&& u) {
  detail::shared_state_ptr<U> state =
    std::make_shared<detail::shared_state<U>>();
  state->set_value(std::forward<U>(u));
  return future<U>(state);
}

template<typename U>
future<U> make_exceptional_future(std::exception_ptr p) {
  detail::shared_state_ptr<U> state =
    std::make_shared<detail::shared_state<U>>();
  state->set_exception(p);
  return future<U>(state);
}

#if !defined (__clang__) && defined (__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 8)
template<typename F>
future<detail::callable_ret_type<F>> async(F&& f) {
  using future_inner_type = detail::callable_ret_type<F>;

  promise<future_inner_type> p;
  auto result = p.get_future();

  std::thread([p = std::move(p), f = std::forward<F>(f)] () mutable {
    try {
      p.set_value(std::forward<F>(f)());
    } catch (...) {
      p.set_exception(std::current_exception());
    }
  }).detach();

  return result;
}

template<typename F, typename Arg1>
future<detail::callable_ret_type<F, Arg1>> async(F&& f, Arg1&& arg1) {
  using future_inner_type = detail::callable_ret_type<F, Arg1>;

  promise<future_inner_type> p;
  auto result = p.get_future();

  std::thread([p = std::move(p), f = std::forward<F>(f), arg1] () mutable {
    try {
      p.set_value(std::forward<F>(f)(arg1));
    } catch (...) {
      p.set_exception(std::current_exception());
    }
  }).detach();

  return result;
}

template<typename F, typename Arg1, typename Arg2>
future<detail::callable_ret_type<F, Arg1, Arg2>> async(F&& f, Arg1&& arg1, Arg2&& arg2) {
  using future_inner_type = detail::callable_ret_type<F, Arg1, Arg2>;

  promise<future_inner_type> p;
  auto result = p.get_future();

  std::thread([p = std::move(p), f = std::forward<F>(f), arg1, arg2] () mutable {
    try {
      p.set_value(std::forward<F>(f)(arg1, arg2));
    } catch (...) {
      p.set_exception(std::current_exception());
    }
  }).detach();

  return result;
}

template<typename Executor, typename F>
future<detail::callable_ret_type<F>> async(Executor& executor, F&& f) {
  using future_inner_type = detail::callable_ret_type<F>;

  auto promise_ptr = std::make_shared<promise<future_inner_type>>();
  auto result = promise_ptr->get_future();
  executor.post([promise_ptr, f = std::forward<F>(f)] () mutable {
    try {
      promise_ptr->set_value(std::forward<F>(f)());
    } catch (...) {
      promise_ptr->set_exception(std::current_exception());
    }
  });

  return result;
}

template<typename Executor, typename F, typename Arg1>
future<detail::callable_ret_type<F, Arg1>> async(Executor& executor, F&& f, Arg1&& arg1) {
  using future_inner_type = detail::callable_ret_type<F, Arg1>;

  auto promise_ptr = std::make_shared<promise<future_inner_type>>();
  auto result = promise_ptr->get_future();
  executor.post([promise_ptr, f = std::forward<F>(f), arg1] () mutable {
    try {
      promise_ptr->set_value(std::forward<F>(f)(arg1));
    } catch (...) {
      promise_ptr->set_exception(std::current_exception());
    }
  });

  return result;
}

template<typename Executor, typename F, typename Arg1, typename Arg2>
future<detail::callable_ret_type<F, Arg1, Arg2>>
async(Executor& executor, F&& f, Arg1&& arg1, Arg2&& arg2) {
  using future_inner_type = detail::callable_ret_type<F, Arg1, Arg2>;

  auto promise_ptr = std::make_shared<promise<future_inner_type>>();
  auto result = promise_ptr->get_future();
  executor.post([promise_ptr, f = std::forward<F>(f), arg1, arg2] () mutable {
    try {
      promise_ptr->set_value(std::forward<F>(f)(arg1, arg2));
    } catch (...) {
      promise_ptr->set_exception(std::current_exception());
    }
  });

  return result;
}
#endif

#if defined (__clang__) || defined(_MSC_VER) || \
    (defined (__GNUC__) && ((__GNUC__ == 4 && __GNUC_MINOR__ >= 9) || __GNUC__ >= 5))

template<typename F, typename... Args>
future<detail::callable_ret_type<F, Args...>> async(F&& f, Args&&... args) {
  using future_inner_type = detail::callable_ret_type<F, Args...>;

  promise<future_inner_type> p;
  auto result = p.get_future();

  std::thread([p = std::move(p), f = std::forward<F>(f), args...] () mutable {
    try {
      p.set_value(std::forward<F>(f)(args...));
    } catch (...) {
      p.set_exception(std::current_exception());
    }
  }).detach();

  return result;
}

template<typename Executor, typename F, typename... Args>
future<detail::callable_ret_type<F, Args...>> async(Executor& executor, F&& f, Args&&... args) {
  using future_inner_type = detail::callable_ret_type<F, Args...>;

  auto promise_ptr = std::make_shared<promise<future_inner_type>>();
  auto result = promise_ptr->get_future();
  executor.post([promise_ptr, f = std::forward<F>(f), args...] () mutable {
    try {
      promise_ptr->set_value(std::forward<F>(f)(args...));
    } catch (...) {
      promise_ptr->set_exception(std::current_exception());
    }
  });

  return result;
}
#endif

template<typename InputIt>
auto when_all(InputIt first, InputIt last)
-> future<std::vector<typename std::iterator_traits<InputIt>::value_type>> {
  using argument_element_type = typename std::iterator_traits<InputIt>::value_type;
  using future_vector_type = std::vector<argument_element_type>;

  struct context {
    size_t total_futures = 0;
    size_t ready_futures = 0;
    future_vector_type future_vector;
    future_vector_type result;
    std::mutex mutex;
    promise<future_vector_type> p;
  };

  auto shared_context = std::make_shared<context>();
  auto result_future = shared_context->p.get_future();
  shared_context->total_futures = std::distance(first, last);
  shared_context->result.resize(shared_context->total_futures);
  size_t index = 0;

  if (shared_context->total_futures == 0)
    return cf::make_ready_future(future_vector_type());

  for (; first != last; ++first, ++index) {
    shared_context->future_vector.emplace_back(std::move(*first));
    shared_context->future_vector[index].then(
      [shared_context, index](argument_element_type f) mutable {
        std::lock_guard<std::mutex> lock(shared_context->mutex);
        shared_context->result[index] = std::move(f);
        ++shared_context->ready_futures;
        if (shared_context->ready_futures == shared_context->total_futures)
          shared_context->p.set_value(std::move(shared_context->result));
        return unit();
      });
  }

  return result_future;
}

template<typename T>
std::vector<T> flat_collect(std::vector<cf::future<T>> vec) {
  std::vector<T> result;
  result.reserve(vec.size());
  for (auto& f: vec)
    result.push_back(f.get());
  return result;
}

namespace detail {
template<size_t I, typename Context, typename Future>
void when_inner_helper(Context context, Future&& f) {
  std::get<I>(context->result) = std::move(f);
  std::get<I>(context->result).then(
      [context](typename std::remove_reference<Future>::type f) {
    std::lock_guard<std::mutex> lock(context->mutex);
    ++context->ready_futures;
    std::get<I>(context->result) = std::move(f);
    if (context->ready_futures == context->total_futures)
      context->p.set_value(std::move(context->result));
    return unit();
  });
}

template<size_t I, typename Context>
void apply_helper(const Context&) {}

template<size_t I, typename Context, typename FirstFuture, typename... Futures>
void apply_helper(const Context& context, FirstFuture&& f, Futures&&... fs) {
  detail::when_inner_helper<I>(context, std::forward<FirstFuture>(f));
  apply_helper<I+1>(context, std::forward<Futures>(fs)...);
}
}

template<typename... Futures>
auto when_all(Futures&&... futures)
-> future<std::tuple<std::decay_t<Futures>...>> {
  using result_inner_type = std::tuple<std::decay_t<Futures>...>;
  struct context {
    size_t total_futures;
    size_t ready_futures = 0;
    result_inner_type result;
    promise<result_inner_type> p;
    std::mutex mutex;
  };
  auto shared_context = std::make_shared<context>();
  shared_context->total_futures = sizeof...(futures);
  detail::apply_helper<0>(shared_context, std::forward<Futures>(futures)...);
  return shared_context->p.get_future();
}

template<typename Sequence>
struct when_any_result {
  size_t index;
  Sequence sequence;
};

template<typename InputIt>
auto when_any(InputIt first, InputIt last)
->future<
    when_any_result<
      std::vector<
        typename std::iterator_traits<InputIt>::value_type>>> {
  using result_inner_type =
    std::vector<typename std::iterator_traits<InputIt>::value_type>;
  using future_inner_type = when_any_result<result_inner_type>;

  struct context {
    size_t total = 0;
    std::atomic<size_t> processed;
    future_inner_type result;
    promise<future_inner_type> p;
    bool ready = false;
    bool result_moved = false;
    std::mutex mutex;
  };

  auto shared_context = std::make_shared<context>();
  auto result_future = shared_context->p.get_future();
  shared_context->processed = 0;
  shared_context->total = std::distance(first, last);
  shared_context->result.sequence.reserve(shared_context->total);
  size_t index = 0;

  auto first_copy = first;
  for (; first_copy != last; ++first_copy) {
    shared_context->result.sequence.push_back(std::move(*first_copy));
  }

  for (; first != last; ++first, ++index) {
    shared_context->result.sequence[index].then(
    [shared_context, index]
    (typename std::iterator_traits<InputIt>::value_type f) mutable {
      {
        std::lock_guard<std::mutex> lock(shared_context->mutex);
        if (!shared_context->ready) {
          shared_context->result.index = index;
          shared_context->ready = true;
          shared_context->result.sequence[index] = std::move(f);
          if (shared_context->processed == shared_context->total &&
              !shared_context->result_moved) {
            shared_context->p.set_value(std::move(shared_context->result));
            shared_context->result_moved = true;
          }
        }
      }
      return unit();
    });
    ++shared_context->processed;
  }

  {
    std::lock_guard<std::mutex> lock(shared_context->mutex);
    if (shared_context->ready && !shared_context->result_moved) {
      shared_context->p.set_value(std::move(shared_context->result));
      shared_context->result_moved = true;
    }
  }

  return result_future;
}

namespace detail {
template<size_t I, typename Context>
void when_any_inner_helper(Context context) {
  using ith_future_type =
    std::decay_t<decltype(std::get<I>(context->result.sequence))>;
  std::get<I>(context->result.sequence).then(
  [context](ith_future_type f) {
    std::lock_guard<std::mutex> lock(context->mutex);
    if (!context->ready) {
      context->ready = true;
      context->result.index = I;
      std::get<I>(context->result.sequence) = std::move(f);
      if (context->processed == context->total &&
          !context->result_moved) {
        context->p.set_value(std::move(context->result));
        context->result_moved = true;
      }
    }
    return unit();
  });
}

template<size_t I, size_t S>
struct when_any_helper_struct {
  template<typename Context, typename... Futures>
  static void apply(const Context& context, std::tuple<Futures...>& t) {
    when_any_inner_helper<I>(context);
    ++context->processed;
    when_any_helper_struct<I+1, S>::apply(context, t);
  }
};

template<size_t S>
struct when_any_helper_struct<S, S> {
  template<typename Context, typename... Futures>
  static void apply(const Context&, std::tuple<Futures...>&) {}
};

template<size_t I, typename Context>
void fill_result_helper(const Context&) {}

template<size_t I, typename Context, typename FirstFuture, typename... Futures>
void fill_result_helper(const Context& context, FirstFuture&& f, Futures&&... fs) {
  std::get<I>(context->result.sequence) = std::move(f);
  fill_result_helper<I+1>(context, std::forward<Futures>(fs)...);
}

template<typename F>
auto make_initiate_handler(F&& f) {
  return [f = std::forward<F>(f)](auto future) mutable {
    future.get();
    return std::move(f)();
  };
}

}

template<typename... Futures>
auto when_any(Futures&&... futures)
-> future<when_any_result<std::tuple<std::decay_t<Futures>...>>> {
  using result_inner_type = std::tuple<std::decay_t<Futures>...>;
  using future_inner_type = when_any_result<result_inner_type>;

  struct context {
    bool ready = false;
    bool result_moved = false;
    size_t total = 0;
    std::atomic<size_t> processed;
    future_inner_type result;
    promise<future_inner_type> p;
    std::mutex mutex;
  };

  auto shared_context = std::make_shared<context>();
  shared_context->processed = 0;
  shared_context->total = sizeof...(futures);

  detail::fill_result_helper<0>(shared_context, std::forward<Futures>(futures)...);
  detail::when_any_helper_struct<0, sizeof...(futures)>::apply(
      shared_context, shared_context->result.sequence);
  {
    std::lock_guard<std::mutex> lock(shared_context->mutex);
    if (shared_context->ready && !shared_context->result_moved) {
      shared_context->p.set_value(std::move(shared_context->result));
      shared_context->result_moved = true;
    }
  }
  return shared_context->p.get_future();
}

template<typename F>
auto initiate(F&& f) {
  return cf::make_ready_future(cf::unit()).then(
    detail::make_initiate_handler(std::forward<F>(f)));
}

template<typename F, typename Executor>
auto initiate(Executor& executor, F&& f) {
  return cf::make_ready_future(cf::unit()).then(executor,
    detail::make_initiate_handler(std::forward<F>(f)));
}

constexpr auto translate_broken_promise_to_operation_canceled = [](auto f) {
  try {
    return f.get();
  } catch (const future_error& e) {
    if (e.ecode() != errc::broken_promise)
      throw;
    throw std::system_error(
      (int) std::errc::operation_canceled, std::generic_category());
  }
};

constexpr auto discard_value = [](auto f) {
  f.get();
  return cf::unit();
};

} // namespace cf
