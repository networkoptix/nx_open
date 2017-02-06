#pragma once

#include <boost/optional.hpp>
#include <chrono>
#include <condition_variable>
#include <future>    //for some enums
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {

//enum class future_status
//{
//    ready,
//    timeout,
//    deferred
//};
//
//enum class future_errc
//{
//    broken_promise,
//    future_already_retrieved,
//    promise_already_satisfied,
//    no_state
//};

//class future_error
//:
//    public std::logic_error
//{
//public:
//    future_error(std::error_code ec)
//    {
//    }
//
//    virtual const char* what() const;
//    const std::error_code& code() const;
//};


namespace detail {

template<typename T>
class shared_future_state_base
{
public:
    shared_future_state_base()
    :
        m_satisfied(false)
    {
    }

    shared_future_state_base(const shared_future_state_base&) = delete;
    shared_future_state_base& operator=(const shared_future_state_base&) = delete;

    void abandon()
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        if (m_satisfied)
            return;
        //storing exception
        set_exception(
            lk,
            std::make_exception_ptr(
                std::future_error(std::future_errc::broken_promise)));
    }

    void set_exception(std::exception_ptr p)
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        set_exception(
            lk,
            std::move(p));
        m_cond.notify_all();
    }

    void wait() const
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        wait(lk);
    }

    template<class Rep, class Period>
    std::future_status wait_for(
        const std::chrono::duration<Rep, Period>& timeout_duration) const
    {
        return wait_until(std::chrono::steady_clock::now() + timeout_duration);
    }

    template<class Clock, class Duration>
    std::future_status wait_until(
        const std::chrono::time_point<Clock, Duration>& timeout_time) const
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        while (!m_satisfied)
        {
            if (m_cond.wait_until(lk, timeout_time) == std::cv_status::timeout)
                return std::future_status::timeout;
        }
        return std::future_status::ready;
    }

protected:
    bool m_satisfied;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cond;
    boost::optional<std::exception_ptr> m_exception;

    void wait(std::unique_lock<std::mutex>& lk) const
    {
        m_cond.wait(
            lk,
            [this] { return m_satisfied; });
    }

    void set_exception(
        std::unique_lock<std::mutex>& /*lk*/,
        std::exception_ptr p)
    {
        if (m_satisfied)
            throw std::future_error(std::future_errc::promise_already_satisfied);
        m_exception = std::move(p);
        m_satisfied = true;
        m_cond.notify_all();
    }
};

template<typename T>
class shared_future_state
:
    public shared_future_state_base<T>
{
public:
    template<class RRef>
    void set_value(RRef&& value)
    {
        std::unique_lock<std::mutex> lk(this->m_mutex);
        if (this->m_satisfied)
            throw std::future_error(std::future_errc::promise_already_satisfied);
        m_value = std::forward<RRef>(value);
        this->m_satisfied = true;
        this->m_cond.notify_all();
    }

    T get()
    {
        std::unique_lock<std::mutex> lk(this->m_mutex);
        this->wait(lk);
        if (this->m_exception)
        {
            auto except = std::move(*this->m_exception);
            this->m_exception.reset();
            std::rethrow_exception(std::move(except));
        }
        auto value = std::move(*m_value);
        m_value.reset();
        return value;
    }

private:
    boost::optional<T> m_value;
};

template<>
class shared_future_state<void>
:
    public shared_future_state_base<void>
{
public:
    void set_value()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (m_satisfied)
            throw std::future_error(std::future_errc::promise_already_satisfied);
        m_satisfied = true;
        m_cond.notify_all();
    }

    void get()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        wait(lk);
        if (m_exception)
        {
            auto except = std::move(*m_exception);
            m_exception.reset();
            std::rethrow_exception(std::move(except));
            return;
        }
        return;
    }
};

}   // namespace detail


template<typename T>
class future
{
    typedef detail::shared_future_state<T> SharedStateType;

public:
    future()
    {
    }

    future(std::shared_ptr<SharedStateType> sharedStatePtr)
    :
        m_sharedState(std::move(sharedStatePtr))
    {
    }

    future(future&& other)
    :
        m_sharedState(std::move(other.m_sharedState))
    {
        other.m_sharedState.reset();
    }

    future(const future& other) = delete;

    ~future()
    {
    }

    future& operator=(future&& other)
    {
        m_sharedState = std::move(other.m_sharedState);
        other.m_sharedState.reset();
        return *this;
    }

    T get()
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        auto sharedState = std::move(m_sharedState);
        m_sharedState.reset();
        return sharedState->get();
    }

    bool valid() const noexcept
    {
        return m_sharedState != nullptr;
    }

    void wait() const
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        m_sharedState->wait();
    }

    template<class Rep, class Period>
    std::future_status wait_for(
        const std::chrono::duration<Rep, Period>& timeout_duration) const
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        return m_sharedState->wait_for(timeout_duration);
    }

    template<class Clock, class Duration>
    std::future_status wait_until(
        const std::chrono::time_point<Clock, Duration>& timeout_time) const
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        return m_sharedState->wait_until(timeout_time);
    }

private:
    std::shared_ptr<SharedStateType> m_sharedState;
};

template<class R>
class promise_base
{
    typedef detail::shared_future_state<R> SharedStateType;

public:
    promise_base()
    :
        m_sharedState(std::make_shared<SharedStateType>()),
        m_futureAlreadyRetrieved(false)
    {
    }

    promise_base(promise_base&& other)
    :
        m_sharedState(std::move(other.m_sharedState)),
        m_futureAlreadyRetrieved(other.m_futureAlreadyRetrieved)
    {
        other.m_sharedState.reset();
        other.m_futureAlreadyRetrieved = false;
    }
    promise_base(const promise_base& other) = delete;

    ~promise_base()
    {
        if (m_sharedState)
            m_sharedState->abandon();
    }

    promise_base& operator=(promise_base&& other)
    {
        m_sharedState = std::move(other.m_sharedState);
        m_futureAlreadyRetrieved = other.m_futureAlreadyRetrieved;

        other.m_sharedState.reset();
        other.m_futureAlreadyRetrieved = false;
        return *this;
    }

    void swap(promise_base& other)
    {
        auto tmpState = std::move(other.m_sharedState);
        other.m_sharedState = std::move(m_sharedState);
        m_sharedState = std::move(tmpState);
        std::swap(m_futureAlreadyRetrieved, other.m_futureAlreadyRetrieved);
    }

    future<R> get_future()
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        if (m_futureAlreadyRetrieved)
            throw std::future_error(std::future_errc::future_already_retrieved);
        m_futureAlreadyRetrieved = true;
        return future<R>(m_sharedState);
    }

    void set_exception(std::exception_ptr p)
    {
        if (!m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        m_sharedState->set_exception(std::move(p));
    }

protected:
    std::shared_ptr<SharedStateType> m_sharedState;
    bool m_futureAlreadyRetrieved;
};

template<class R>
class promise
:
    public promise_base<R>
{
public:
    promise() = default;
    promise(promise&&) = default;
    promise& operator=(promise&&) = default;

    void set_value(const R& value)
    {
        if (!this->m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        this->m_sharedState->set_value(value);
    }

    void set_value(R&& value)
    {
        if (!this->m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        this->m_sharedState->set_value(std::move(value));
    }
};

template<>
class promise<void>
:
    public promise_base<void>
{
public:
    promise() = default;
    promise(promise&&) = default;
    promise& operator=(promise&&) = default;

    void set_value()
    {
        if (!this->m_sharedState)
            throw std::future_error(std::future_errc::no_state);
        this->m_sharedState->set_value();
    }
};


//TODO std::swap(promise_base& one, promise_base& two);

//TODO promise_base<void>

}   // namespace utils
}   // namespace nx
