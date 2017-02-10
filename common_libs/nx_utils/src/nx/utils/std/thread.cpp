#include "thread.h"

#include <nx/utils/thread/thread_util.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

namespace detail {

thread::thread(nx::utils::MoveOnlyFunc<void()> threadFunc) noexcept(false):
    m_threadFunc(std::move(threadFunc))
{
    start();
    if (!isRunning())
        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));

    m_idFilledPromise.get_future().wait();
}

void thread::join() noexcept(false)
{
    if (get_id() == std::this_thread::get_id())
        throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

    wait();
    m_id = std::thread::id();
    m_nativeHandle = 0;
}

std::thread::id thread::get_id() const
{
    return m_id;
}

uintptr_t thread::native_handle() const
{
    return m_nativeHandle;
}

void thread::run()
{
    m_id = std::this_thread::get_id();
    m_nativeHandle = currentThreadSystemId();
    m_idFilledPromise.set_value();
    m_threadFunc();
}

} // namespace detail

thread::thread(thread&& other) noexcept:
    m_actualThread(std::move(other.m_actualThread))
{
    other.m_actualThread = std::unique_ptr<detail::thread>();
}

thread::~thread()
{
    if (joinable())
        std::terminate();
}

thread& thread::operator=(thread&& other) noexcept
{
    if (this != &other)
    {
        m_actualThread = std::move(other.m_actualThread);
        other.m_actualThread = std::unique_ptr<detail::thread>();
    }

    return *this;
}

bool thread::joinable() const noexcept
{
    return get_id() != id();
}

thread::id thread::get_id() const noexcept
{
    if (!m_actualThread)
        return id();

    return m_actualThread->get_id();
}

thread::native_handle_type thread::native_handle() noexcept
{
    if (!m_actualThread)
        return (native_handle_type)nullptr;

    return (native_handle_type)m_actualThread->native_handle();
}

void thread::join() noexcept(false)
{
    if (!joinable())
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));

    m_actualThread->join();
}

class DetachedThreads
{
    DetachedThreads():
        m_collector(
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                while (!m_isTerminated)
                {
                    {
                        QnMutexUnlocker unlock(&lock);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }

                    for (auto it = m_garbage.begin(); it != m_garbage.end(); )
                    {
                        if ((*it)->isFinished())
                            it = m_garbage.erase(it);
                        else
                            ++it;
                    }
                }
            })
    {
    }

    ~DetachedThreads()
    {
        {
            QnMutexLocker lk(&m_mutex);
            m_isTerminated = true;
        }

        m_collector.join();
        for (auto& thread: m_garbage)
            thread->join();
    }

public:
    void addThread(std::unique_ptr<detail::thread> thread)
    {
        QnMutexLocker lk(&m_mutex);
        m_garbage.push_back(std::move(thread));
    }

    static DetachedThreads* instance()
    {
        static DetachedThreads object;
        return &object;
    }

private:
    QnMutex m_mutex;
    bool m_isTerminated = false;
    std::list<std::unique_ptr<detail::thread>> m_garbage;
    std::thread m_collector;
};

void thread::detach() throw (std::system_error)
{
    if (!joinable())
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));

    DetachedThreads::instance()->addThread(std::move(m_actualThread));
}

void thread::swap(thread& other) noexcept
{
    auto bak = std::move(other);
    other = std::move(*this);
    *this = std::move(bak);
}

unsigned int thread::hardware_concurrency() noexcept
{
    return std::thread::hardware_concurrency();
}

} // namespace utils
} // namespace nx
