// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread.h"

#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>

#include "thread_util.h"

namespace nx {
namespace utils {

/*static*/ int Thread::s_threadStackSize = 0;

Thread::Thread()
{
    connect(this, &QThread::started, this, &Thread::at_started, Qt::DirectConnection);
    connect(this, &QThread::finished, this, &Thread::at_finished, Qt::DirectConnection);

    #if !defined(Q_OS_ANDROID) //< Not supported on Android.
        if (s_threadStackSize > 0)
            setStackSize(s_threadStackSize);
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

ThreadId Thread::systemThreadId() const
{
    return m_systemThreadId;
}

void Thread::initSystemThreadId()
{
    if (m_systemThreadId)
        return;

    m_systemThreadId = currentThreadSystemId();
}

ThreadId Thread::currentThreadSystemId()
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
    NX_VERBOSE(this, __func__);
    m_needStop = true;
    if (m_onPause)
        resume();
}

void Thread::stop()
{
    NX_VERBOSE(this, __func__);
#if defined(_DEBUG)
    if (m_type)
    {
        NX_ASSERT(
            typeid(*this) == *m_type,
            nx::format("stop() must be called from derived class destructor. This: %1, m_type: %2").args(
                typeid(*this), *m_type)
        );
        m_type = nullptr; //< So that we don't check it again.
    }
#endif
    pleaseStop();
    wait();
}

void Thread::at_started()
{
    initSystemThreadId();

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
