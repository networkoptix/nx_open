// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "elapsed_timer_thread_safe.h"

#include <mutex>

namespace nx {
namespace utils {

void ElapsedTimerThreadSafe::start()
{
    NX_WRITE_LOCKER lock(&m_mutex);
    m_timer.start();
}

void ElapsedTimerThreadSafe::stop()
{
    NX_WRITE_LOCKER lock(&m_mutex);
    m_timer.invalidate();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsedSinceStart() const
{
    NX_READ_LOCKER lock(&m_mutex);
    return std::chrono::milliseconds(m_timer.isValid() ? m_timer.elapsed() : 0);
}

bool ElapsedTimerThreadSafe::hasExpiredSinceStart(std::chrono::milliseconds ms) const
{
    NX_READ_LOCKER lock(&m_mutex);
    return m_timer.isValid() && m_timer.hasExpired(ms.count());
}

bool ElapsedTimerThreadSafe::isStarted() const
{
    NX_READ_LOCKER lock(&m_mutex);
    return m_timer.isValid();
}

std::chrono::milliseconds ElapsedTimerThreadSafe::elapsed() const
{
    NX_READ_LOCKER lock(&m_mutex);
    return std::chrono::milliseconds(m_timer.elapsed());
}

bool ElapsedTimerThreadSafe::hasExpired(std::chrono::milliseconds ms) const
{
    NX_READ_LOCKER lock(&m_mutex);
    return m_timer.hasExpired(ms.count());
}

} // namespace utils
} // namespace nx
