#pragma once

#include <functional>
#include <thread>
#include <system_error>

#include <QThread>

#include <nx/utils/std/cpp14.h>

#include "future.h"
#include "../move_only_func.h"
#include "../type_utils.h"

#include <nx/utils/singleton.h>

namespace nx {
namespace utils {

namespace detail {

class NX_UTILS_API thread
:
    public QThread
{
public:
    thread(nx::utils::MoveOnlyFunc<void()> threadFunc) noexcept(false);
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;

    void join() noexcept(false);
    std::thread::id get_id() const;
    uintptr_t native_handle() const;

private:
    promise<void> m_idFilledPromise;
    std::thread::id m_id;
    uintptr_t m_nativeHandle;
    nx::utils::MoveOnlyFunc<void()> m_threadFunc;

    virtual void run() override;
};

}   //namespace detail

class NX_UTILS_API thread
{
public:
    typedef std::thread::id id;
    typedef std::thread::native_handle_type native_handle_type;

    thread() noexcept {}
    thread(thread&& other) noexcept;

    template< class Function, class... Args >
        explicit thread(Function&& f, Args&&... args) noexcept(false)
    {
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        m_actualThread = std::make_unique<detail::thread>(
            [f1 = std::forward<Function>(f), argsTuple = std::move(argsTuple)]() mutable
            {
                expandTupleIntoArgs(std::move(f1), std::move(argsTuple));
            });
    }

    thread(const thread&) = delete;
    ~thread();
    thread& operator=(thread&& other) noexcept;

    bool joinable() const noexcept;
    id get_id() const noexcept;
    native_handle_type native_handle() noexcept;
    void join() noexcept(false);
    void detach() noexcept (false);
    void swap(thread& other) noexcept;

    static unsigned int hardware_concurrency() noexcept;

private:
    std::unique_ptr<detail::thread> m_actualThread;
};

class NX_UTILS_API DetachedThreads: public Singleton<DetachedThreads>
{
public:
    DetachedThreads();
    ~DetachedThreads();

    void addThread(std::unique_ptr<detail::thread> thread);

private:
    utils::promise<void> m_stopPromise;
    std::mutex m_mutex;
    std::list<std::unique_ptr<detail::thread>> m_garbage;
    thread m_collector;
};

}   //namespace utils
}   //namespace nx
