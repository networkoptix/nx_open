// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <coroutine>
#include <exception>
#include <functional>
#include <optional>

namespace nx::coro {

// Special exception class to be thrown when a task is cancelled. Used to destruct coroutine stack
// frames until top level coroutine suppresses this exception. Other exceptions terminate the app.
class NX_UTILS_API TaskCancelException: public std::exception
{
public:
    const char* what() const noexcept override
    {
        return "Task cancel exception";
    }
};

// Base class for coroutine composition. Resumes the awaiting coroutine when current coroutine
// completes.
template<
    typename T = void,
    typename InitialSuspend = std::suspend_always,
    bool destroyOnReturn = false
>
class TaskBase
{
public:
    struct PromiseBase
    {
        friend struct FinalAwaiter;

        struct FinalAwaiter
        {
            bool await_ready() const noexcept { return false; }

            template<typename Promise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coro) noexcept
            {
                if (auto e = coro.promise().m_exception)
                {
                    try
                    {
                        std::rethrow_exception(e);
                    }
                    catch (const TaskCancelException&)
                    {
                        // Cancel exception is suppressed.
                    }
                    catch (...)
                    {
                        if (!coro.promise().m_continuation)
                            std::terminate();
                    }
                }

                if (auto continuation = coro.promise().m_continuation)
                    return continuation;

                return std::noop_coroutine();
            }

            void await_resume() noexcept {}
        };

        struct FinalAwaiterDestroyOnReturn
        {
            bool await_ready() const noexcept { return false; }

            template<typename Promise>
            void await_suspend(std::coroutine_handle<Promise> coro) noexcept
            {
                if (auto e = coro.promise().m_exception)
                {
                    try
                    {
                        std::rethrow_exception(e);
                    }
                    catch (const TaskCancelException&)
                    {
                        // Cancel exception is suppressed.
                    }
                    catch (...)
                    {
                        std::terminate();
                    }
                }

                coro.destroy();
            }

            void await_resume() noexcept {}
        };

        InitialSuspend initial_suspend() noexcept { return {}; }

        using FinalAwaiterType = std::conditional_t<
            destroyOnReturn,
            FinalAwaiterDestroyOnReturn,
            FinalAwaiter>;

        FinalAwaiterType final_suspend() noexcept { return {}; }

        void unhandled_exception() noexcept { m_exception = std::current_exception(); }

        void addCancelCondition(std::function<bool()> cancelOnResume)
        {
            if (!m_cancelOnResume)
            {
                m_cancelOnResume = std::move(cancelOnResume);
                return;
            }

            auto prevCancelOnResume = std::move(m_cancelOnResume);
            m_cancelOnResume = [prevCancelOnResume = std::move(prevCancelOnResume),
                cancelOnResume = std::move(cancelOnResume)]() mutable
                {
                    return prevCancelOnResume() || cancelOnResume();
                };
        }

        std::coroutine_handle<> m_continuation;
        std::exception_ptr m_exception;
        std::function<bool()> m_cancelOnResume;
    };

    struct PromiseVoid: public PromiseBase
    {
        auto get_return_object() { return coroutine_handle_t::from_promise(*this); }

        void return_void() noexcept {}

        void result() {}
    };

    struct PromiseT: public PromiseBase
    {
        auto get_return_object() { return coroutine_handle_t::from_promise(*this); }

        void return_value(T&& v) { value = std::move(v); }

        void return_value(const T& v) { value = v; }

        T&& result() { return std::move(*value); }

        std::optional<T> value; // Allow non-default-constructible T (e.g. std::expected).
    };

    using promise_type = std::conditional_t<std::is_same_v<T, void>, PromiseVoid, PromiseT>;

    using coroutine_handle_t = std::coroutine_handle<promise_type>;

protected:
    struct Awaiter
    {
        coroutine_handle_t m_coroutine;

        Awaiter(coroutine_handle_t coroutine) noexcept: m_coroutine(coroutine) {}

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
        {
            m_coroutine.promise().m_continuation = awaitingCoroutine;

            if (!m_coroutine.promise().m_cancelOnResume)
            {
                auto& promise = std::coroutine_handle<PromiseBase>::from_address(
                    awaitingCoroutine.address()).promise();

                // Inherit cancel condition from the awaiting coroutine.
                m_coroutine.promise().addCancelCondition(promise.m_cancelOnResume);
            }

            return m_coroutine;
        }

        auto await_resume()
        {
            if (m_coroutine.promise().m_exception)
                std::rethrow_exception(std::exchange(m_coroutine.promise().m_exception, {}));

            auto& continuationPromise = std::coroutine_handle<PromiseBase>::from_address(
                m_coroutine.promise().m_continuation.address()).promise();

            if (const auto r = continuationPromise.m_cancelOnResume; r && r())
                throw TaskCancelException();

            return std::move(m_coroutine.promise()).result();
        }
    };
};

// Lazy task return type that transforms a function into a coroutine.
template<typename T = void>
class [[nodiscard]] Task: public TaskBase<T, std::suspend_always>
{
    using base_type = TaskBase<T, std::suspend_always>;

public:
    using result_t = T;

    Task(typename base_type::coroutine_handle_t coroutine): m_coroutine(coroutine) {}

    Task() {}

    bool isNull() const noexcept { return !static_cast<bool>(m_coroutine); }

    Task(Task&& other)
    {
        m_coroutine = std::move(other.m_coroutine);
        other.m_coroutine = {};
    }

    Task& operator=(Task&& other)
    {
        m_coroutine = std::move(other.m_coroutine);
        other.m_coroutine = {};
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task()
    {
        if (m_coroutine)
            m_coroutine.destroy();
    }

    void start(std::function<bool()> cancelOnResume = {}) noexcept
    {
        if (m_coroutine && !m_coroutine.done())
        {
            if (cancelOnResume)
                m_coroutine.promise().addCancelCondition(std::move(cancelOnResume));
            m_coroutine.resume();
        }
    }

    void addCancelCondition(std::function<bool()> cancelOnResume) noexcept
    {
        if (m_coroutine && !m_coroutine.done())
            m_coroutine.promise().addCancelCondition(std::move(cancelOnResume));
    }

    bool done() const { return m_coroutine && m_coroutine.done(); }

    T result() { return std::move(m_coroutine.promise().result()); }

    auto operator co_await() const noexcept { return awaiter{m_coroutine}; }

    auto handle() const noexcept { return m_coroutine; }

protected:
    using awaiter = typename base_type::Awaiter;

private:
    typename base_type::coroutine_handle_t m_coroutine = nullptr;
};

// Eager task that destroys itself after returning from the coroutine. Useful for starting
// a coroutine in regular code flow.
class FireAndForget: public TaskBase<void, std::suspend_never, true /*destroyOnReturn*/>
{
    using base_type = TaskBase<void, std::suspend_never, true /*destroyOnReturn*/>;

public:
    FireAndForget(typename base_type::coroutine_handle_t) {}
};

static inline bool isCancelRequested(std::coroutine_handle<> h)
{
    const auto request = std::coroutine_handle<TaskBase<>::PromiseBase>::from_address(
        h.address()).promise().m_cancelOnResume;
    return request && request();
}

[[nodiscard]] static inline auto cancelIf(std::function<bool()> condition)
{
    struct AddConditionAwaiter
    {
        AddConditionAwaiter(std::function<bool()> condition): m_condition(std::move(condition))
        {
        }

        bool await_ready() const { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
        {
            auto& promise = std::coroutine_handle<TaskBase<>::PromiseBase>::from_address(
                h.address()).promise();
            promise.addCancelCondition(m_condition);
            return h;
        }

        void await_resume() const
        {
            if (m_condition())
                throw TaskCancelException();
        }

        std::function<bool()> m_condition;
    };

    return AddConditionAwaiter{std::move(condition)};
}

} // namespace nx::coro
