// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

// This file is combined from several files in https://github.com/jbaldwin/libcoro

#pragma once

#include <atomic>

#include "task.h"

namespace nx::coro {

namespace detail {

template<typename Callback>
class OnScopeExit
{
public:
    OnScopeExit(Callback callback): m_callback(std::move(callback)) {}
    virtual ~OnScopeExit() { if (m_callback) (*m_callback)(); }
    void disarm() { m_callback = std::nullopt; }
    OnScopeExit(const OnScopeExit&) = delete;
    OnScopeExit& operator=(const OnScopeExit&) = delete;
    OnScopeExit(OnScopeExit&& rhs) = delete;
    OnScopeExit& operator=(OnScopeExit&& rhs) = delete;
private:
    std::optional<Callback> m_callback;
};

template<typename... Tasks>
struct AllTasksAwaiter
{
    AllTasksAwaiter(std::tuple<Tasks...> tasks):
        m_tasks(std::move(tasks)),
        m_waiting(std::tuple_size_v<decltype(tasks)>)
    {
    }

    bool await_ready() const { return false; }

    template<size_t I>
    void startAt(std::coroutine_handle<> h)
    {
        // Do not capture `this` because lambda captures are destroyed when it goes out of scope.
        [](auto self, std::coroutine_handle<> h) -> nx::coro::FireAndForget
        {
            OnScopeExit defer(
                [self, h]() mutable
                {
                    // Last finished task resumes the awaiting coroutine.
                    if (self->m_waiting.fetch_sub(1, std::memory_order::acq_rel) == 1)
                        h();
                });

            std::get<I>(self->m_results) = co_await std::get<I>(self->m_tasks);
        }(this, h);
    }

    // TODO: replace with C++26 Expansion statements.
    template<std::size_t I = 0>
    void startAll(std::coroutine_handle<> h)
    {
        startAt<I>(h);
        if constexpr(I + 1 != std::tuple_size_v<decltype(m_tasks)>)
            startAll<I + 1>(h);
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        // Iterate over the tasks tuple and start each task.
        startAll(h);
    }

    auto await_resume()
    {
        // Throw cancel exception if any of the results are empty.
        auto cancelIfEmpty = [](const auto& arg) { if (!arg) throw TaskCancelException(); };
        std::apply([cancelIfEmpty](const auto&... args) {(cancelIfEmpty(args), ...);}, m_results);

        return std::apply(
            [](auto&&... args) { return std::make_tuple(std::move(*args)...); },
            std::move(m_results));
    }

    std::tuple<std::optional<typename Tasks::result_t>...> m_results;
    std::tuple<Tasks...> m_tasks;
    std::atomic<std::size_t> m_waiting;
};

} // namespace detail

template<typename... Tasks>
Task<std::tuple<typename Tasks::result_t...>> whenAll(Tasks... tasks)
{
    // Works like co_return std::make_tuple(co_await std::move(tasks)...) but concurrently.
    co_return co_await detail::AllTasksAwaiter{std::make_tuple(std::move(tasks)...)};
}

} // namespace nx::coro
