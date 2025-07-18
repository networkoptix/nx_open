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

// Awaiter for a tuple of tasks.
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

            using AwaitingPromise =
                std::tuple_element<I, decltype(self->m_tasks)>::type::PromiseBase;
            auto& promise = std::coroutine_handle<AwaitingPromise>::from_address(
                h.address()).promise();

            // Inherit cancel condition from the awaiting coroutine.
            if (promise.m_cancelOnResume)
                co_await cancelIf(promise.m_cancelOnResume);

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

// Awaiter for a vector of tasks, with a limit on the number of concurrent tasks.
// If limit is 0, all tasks are started concurrently.
template<typename T>
struct TaskListAwaiter
{
    TaskListAwaiter(std::vector<Task<T>> tasks, size_t limit):
        m_tasks(std::move(tasks)),
        m_limit(std::min(limit, m_tasks.size())),
        m_results(m_tasks.size(), std::nullopt)
    {
        m_waiting = m_tasks.size();

        if (m_limit == 0)
            m_limit = m_tasks.size();
        m_next = m_limit;
    }

    bool await_ready() const { return m_tasks.empty(); }

    // Get next task index to start, avoiding integer overflow.
    std::optional<size_t> getNextIndex()
    {
        size_t newNext = 0;
        size_t next = 0;

        do
        {
            next = m_next;
            if (next == std::numeric_limits<size_t>::max() || next >= m_tasks.size())
                return {};
            newNext = next + 1;
        } while(!m_next.compare_exchange_weak(next, newNext));

        return next;
    }

    nx::coro::FireAndForget startAt(size_t i, std::coroutine_handle<> h)
    {
        OnScopeExit defer(
            [this, h]() mutable
            {
                if (auto next = getNextIndex())
                    startAt(*next, h);

                // Last finished task resumes the awaiting coroutine.
                if (m_waiting.fetch_sub(1, std::memory_order::acq_rel) == 1)
                    h();
            });

        auto& promise = std::coroutine_handle<typename Task<T>::PromiseBase>::from_address(
            h.address()).promise();

        // Inherit cancel condition from the awaiting coroutine.
        if (promise.m_cancelOnResume)
            co_await cancelIf(promise.m_cancelOnResume);

        m_results[i] = co_await m_tasks[i];
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        // startAt() may destroy `this` when resuming the coroutine, but `m_limit` have to be used
        // in the loop condition, so save `limit` on stack.
        auto limit = m_limit;
        for (size_t i = 0; i < limit; ++i)
            startAt(i, h);
    }

    auto await_resume()
    {
        for (const auto& result: m_results)
        {
            if (!result)
                throw TaskCancelException();
        }

        std::vector<T> results;
        results.reserve(m_tasks.size());
        for (auto& result: m_results)
            results.emplace_back(std::move(*result));

        return results;
    }

    std::vector<Task<T>> m_tasks;
    size_t m_limit;
    std::vector<std::optional<T>> m_results;
    std::atomic<std::size_t> m_waiting;
    std::atomic<std::size_t> m_next;
};

} // namespace detail

template<typename... Tasks>
Task<std::tuple<typename Tasks::result_t...>> whenAll(Tasks... tasks)
{
    // Works like co_return std::make_tuple(co_await std::move(tasks)...) but concurrently.
    co_return co_await detail::AllTasksAwaiter{std::make_tuple(std::move(tasks)...)};
}

template<typename T>
Task<std::vector<T>> runAll(std::vector<Task<T>> tasks, size_t limit = 0)
{
    co_return co_await detail::TaskListAwaiter(std::move(tasks), limit);
}

} // namespace nx::coro
