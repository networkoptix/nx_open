#include "thread.h"

#include <nx/utils/crash_dump/systemexcept.h>

#include "thread_util.h"

namespace nx {
namespace utils {

Thread::Thread()
{
    connect(this, &QThread::started, this, &Thread::at_started, Qt::DirectConnection);
    connect(this, &QThread::finished, this, &Thread::at_finished, Qt::DirectConnection);

#if !defined(Q_OS_ANDROID) //< Not supported on Android.
    // Thread stack size reduced (from default 8MB on Linux) to save memory on edge devices.
    // If this is not enough consider using heap.

    // This value is not platform-dependent to be able to catch stack overflow error on every
    // platform.

    constexpr size_t kDefaultThreadStackSize = 128 * 1024;
    setStackSize(kDefaultThreadStackSize);
#endif
}

Thread::~Thread()
{
    NX_ASSERT(!isRunning(), "Thread is destroyed without a call to stop().");
}

bool Thread::needToStop() const
{
    return m_needStop;
}

void Thread::pause()
{
    m_semaphore.tryAcquire(m_semaphore.available());
    m_onPause = true;
}

void Thread::resume()
{
    m_onPause = false;
    m_semaphore.release();
}

bool Thread::isPaused() const
{
    return m_onPause;
}

void Thread::pauseDelay()
{
    while (m_onPause && !needToStop())
    {
        emit paused();
        m_semaphore.tryAcquire(1, 50);
    }
}

uintptr_t Thread::systemThreadId() const
{
    return m_systemThreadId;
}

void Thread::initSystemThreadId()
{
    if (m_systemThreadId)
        return;

    m_systemThreadId = currentThreadSystemId();
}

uintptr_t Thread::currentThreadSystemId()
{
    return ::currentThreadSystemId();
}

void Thread::start(Priority priority)
{
    if (isRunning())
        return;

#if defined(_DEBUG)
    m_type = &typeid(*this);
#endif

    m_needStop = false;
    QThread::start(priority);
}

void Thread::pleaseStop()
{
    m_needStop = true;
    if (m_onPause)
        resume();
}

void Thread::stop()
{
#if defined(_DEBUG)
    if (m_type)
    {
        NX_ASSERT(typeid(*this) == *m_type,
            "stop() must be called from derived class destructor.");
        m_type = nullptr; //< So that we don't check it again.
    }
#endif

    pleaseStop();
    wait();
}

void Thread::at_started()
{
    initSystemThreadId();

#ifdef _WIN32
    win32_exception::installThreadSpecificUnhandledExceptionHandler();
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    linux_exception::installQuitThreadBacktracer();
#endif
}

void Thread::at_finished()
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    linux_exception::uninstallQuitThreadBacktracer();
#endif
}

} // namespace utils
} // namespace nx
