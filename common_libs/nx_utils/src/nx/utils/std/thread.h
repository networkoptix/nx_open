/**********************************************************
* Apr 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <thread>
#include <system_error>

#include <utils/common/long_runnable.h>

#include "../future.h"
#include "../move_only_func.h"
#include "../type_utils.h"


namespace nx {
namespace utils {

namespace detail {

class thread
:
    public QnLongRunnable
{
public:
    thread(nx::utils::MoveOnlyFunc<void()> threadFunc) throw(std::system_error)
    :
        m_threadFunc(std::move(threadFunc))
    {
        start();
        if (!isRunning())
            throw std::system_error(
                std::make_error_code(std::errc::resource_unavailable_try_again));
        m_idFilledPromise.get_future().wait();
    }

    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;

    void join() throw (std::system_error)
    {
        if (get_id() == std::this_thread::get_id())
            throw std::system_error(
                std::make_error_code(std::errc::resource_deadlock_would_occur));
        stop();
        m_id = std::thread::id();
    }

    std::thread::id get_id() const
    {
        return m_id;
    }

private:
    promise<void> m_idFilledPromise;
    std::thread::id m_id;
    nx::utils::MoveOnlyFunc<void()> m_threadFunc;

    virtual void run() override
    {
        initSystemThreadId();
        m_id = std::this_thread::get_id();
        m_idFilledPromise.set_value();

        m_threadFunc();
    }
};

}   //namespace detail

class thread
:
    private QnLongRunnable
{
public:
    typedef std::thread::id id;
    typedef std::thread::native_handle_type native_handle_type;

    thread() noexcept {}

    thread(thread&& other) noexcept
    :
        m_actualThread(std::move(other.m_actualThread))
    {
        other.m_actualThread = std::unique_ptr<detail::thread>();
    }

    template< class Function, class... Args >
        explicit thread(Function&& f, Args&&... args) throw(std::system_error)
    {
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        m_actualThread = std::make_unique<detail::thread>(
            [f1 = std::forward<Function>(f), argsTuple = std::move(argsTuple)]() mutable
            {
                expandTupleIntoArgs(std::move(f1), std::move(argsTuple));
            });
    }

    thread(const thread&) = delete;

    ~thread()
    {
        if (joinable())
            std::terminate();
    }

    thread& operator=(thread&& other)
    {
        if (this != &other)
        {
            m_actualThread = std::move(other.m_actualThread);
            other.m_actualThread = std::unique_ptr<detail::thread>();
        }
        return *this;
    }

    bool joinable() const noexcept
    {
        return get_id() != id();
    }

    id get_id() const noexcept
    {
        if (!m_actualThread)
            return id();
        return m_actualThread->get_id();
    }

    native_handle_type native_handle()
    {
        if (!m_actualThread)
            return (native_handle_type)nullptr;

        return (native_handle_type)m_actualThread->systemThreadId();
    }

    void join() throw (std::system_error)
    {
        if (!joinable())
            throw std::system_error(
                std::make_error_code(std::errc::invalid_argument));
        m_actualThread->join();
    }

    //void detach() throw (std::system_error);

    void swap(thread& other) noexcept
    {
        auto bak = std::move(other);
        other = std::move(*this);
        *this = std::move(bak);
    }

    static unsigned int hardware_concurrency() noexcept
    {
        return std::thread::hardware_concurrency();
    }

private:
    std::unique_ptr<detail::thread> m_actualThread;
};

}   //namespace utils
}   //namespace nx
