// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <version>

#if defined(__cpp_lib_jthread)

    #include <stop_token>
    #include <thread>

namespace nx::utils {

inline constexpr bool kUseStdJThread = true;

using std::jthread;
using std::nostopstate;
using std::nostopstate_t;
using std::stop_source;
using std::stop_token;

} // namespace nx::utils

#else

    #include <atomic>
    #include <memory>
    #include <thread>
    #include <type_traits>
    #include <utility>

namespace nx::utils {

inline constexpr bool kUseStdJThread = false;

namespace detail {

struct StopState
{
    std::atomic<bool> stopRequested = false;
    std::atomic<int> sourceCount = 0;
};

} // namespace detail

struct nostopstate_t
{
    explicit nostopstate_t() = default;
};

inline constexpr nostopstate_t nostopstate{};

class stop_token
{
public:
    stop_token() noexcept = default;

    bool stop_requested() const noexcept
    {
        return m_state && m_state->stopRequested.load(std::memory_order_acquire);
    }

    bool stop_possible() const noexcept
    {
        return m_state
            && (m_state->stopRequested.load(std::memory_order_acquire)
                || m_state->sourceCount.load(std::memory_order_acquire) > 0);
    }

    void swap(stop_token& other) noexcept { m_state.swap(other.m_state); }

    friend bool operator==(const stop_token& lhs, const stop_token& rhs) noexcept = default;
    friend void swap(stop_token& lhs, stop_token& rhs) noexcept { lhs.swap(rhs); }

private:
    friend class stop_source;

    explicit stop_token(std::shared_ptr<detail::StopState> state) noexcept:
        m_state(std::move(state))
    {
    }

private:
    std::shared_ptr<detail::StopState> m_state;
};

class stop_source
{
public:
    stop_source(): m_state(std::make_shared<detail::StopState>())
    {
        m_state->sourceCount.fetch_add(1, std::memory_order_relaxed);
    }

    explicit stop_source(nostopstate_t) noexcept {}

    stop_source(const stop_source& other) noexcept: m_state(other.m_state)
    {
        if (m_state)
            m_state->sourceCount.fetch_add(1, std::memory_order_relaxed);
    }

    stop_source(stop_source&& other) noexcept: m_state(std::move(other.m_state)) {}

    ~stop_source()
    {
        if (m_state)
            m_state->sourceCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    stop_source& operator=(const stop_source& other) noexcept
    {
        stop_source(other).swap(*this);
        return *this;
    }

    stop_source& operator=(stop_source&& other) noexcept
    {
        stop_source(std::move(other)).swap(*this);
        return *this;
    }

    bool request_stop() noexcept
    {
        return m_state && !m_state->stopRequested.exchange(true, std::memory_order_acq_rel);
    }

    stop_token get_token() const noexcept { return stop_token(m_state); }

    bool stop_requested() const noexcept
    {
        return m_state && m_state->stopRequested.load(std::memory_order_acquire);
    }

    bool stop_possible() const noexcept { return static_cast<bool>(m_state); }

    void swap(stop_source& other) noexcept { m_state.swap(other.m_state); }

    friend bool operator==(const stop_source& lhs, const stop_source& rhs) noexcept = default;
    friend void swap(stop_source& lhs, stop_source& rhs) noexcept { lhs.swap(rhs); }

private:
    std::shared_ptr<detail::StopState> m_state;
};

class jthread
{
public:
    using id = std::thread::id;
    using native_handle_type = std::thread::native_handle_type;

    jthread() noexcept: m_stopSource(nostopstate) {}

    template<typename Function, typename... Args>
        requires(!std::is_same_v<std::remove_cvref_t<Function>, jthread>)
    explicit jthread(Function&& function, Args&&... args):
        m_thread(start(std::forward<Function>(function), std::forward<Args>(args)...))
    {
    }

    jthread(const jthread&) = delete;
    jthread& operator=(const jthread&) = delete;

    jthread(jthread&& other) noexcept = default;

    jthread& operator=(jthread&& other) noexcept
    {
        if (this != &other)
        {
            stopAndJoin();
            m_stopSource = std::move(other.m_stopSource);
            m_thread = std::move(other.m_thread);
        }
        return *this;
    }

    ~jthread() { stopAndJoin(); }

    bool joinable() const noexcept { return m_thread.joinable(); }
    void join() { m_thread.join(); }
    void detach() { m_thread.detach(); }

    id get_id() const noexcept { return m_thread.get_id(); }
    native_handle_type native_handle() { return m_thread.native_handle(); }

    stop_source get_stop_source() noexcept { return m_stopSource; }
    stop_token get_stop_token() const noexcept { return m_stopSource.get_token(); }
    bool request_stop() noexcept { return m_stopSource.request_stop(); }

    void swap(jthread& other) noexcept
    {
        m_stopSource.swap(other.m_stopSource);
        m_thread.swap(other.m_thread);
    }

    friend void swap(jthread& lhs, jthread& rhs) noexcept { lhs.swap(rhs); }

    static unsigned int hardware_concurrency() noexcept
    {
        return std::thread::hardware_concurrency();
    }

private:
    template<typename Function, typename... Args>
    std::thread start(Function&& function, Args&&... args)
    {
        if constexpr (std::is_invocable_v<std::decay_t<Function>,
                          stop_token,
                          std::decay_t<Args>...>)
        {
            return std::thread(std::forward<Function>(function),
                m_stopSource.get_token(),
                std::forward<Args>(args)...);
        }
        else
        {
            return std::thread(std::forward<Function>(function), std::forward<Args>(args)...);
        }
    }

    void stopAndJoin()
    {
        if (!joinable())
            return;

        request_stop();
        join();
    }

private:
    stop_source m_stopSource;
    std::thread m_thread;
};

} // namespace nx::utils

#endif
