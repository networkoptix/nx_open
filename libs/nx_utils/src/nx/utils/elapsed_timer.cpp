// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "elapsed_timer.h"

namespace nx {
namespace utils {

using namespace std::chrono;

SafeElapsedTimer::SafeElapsedTimer(ElapsedTimerState state):
    base_type(state)
{
}

bool SafeElapsedTimer::hasExpired(std::chrono::milliseconds value) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::hasExpired(value);
}

std::chrono::milliseconds SafeElapsedTimer::restart()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::restart();
}

void SafeElapsedTimer::invalidate()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    base_type::invalidate();
}

bool SafeElapsedTimer::isValid() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::isValid();
}

std::chrono::milliseconds SafeElapsedTimer::elapsed() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::elapsed();
}

} // namespace utils
} // namespace nx
