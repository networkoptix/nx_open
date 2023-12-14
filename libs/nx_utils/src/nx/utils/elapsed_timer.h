// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

namespace nx {
namespace utils {

enum class ElapsedTimerState
{
    invalid,
    started,
};

/**
 * Calculates elapsed time using nx::utils::monotonicTime.
 * @param Duration std::chrono duration type.
 * NOTE: isValid() returns false just after construction. So, restart() has to be invoked.
 */
template<typename Duration>
class BasicElapsedTimer
{
public:
    /**
     * @param state Defaulted to ElapsedTimerState::invalid to preserve existing behavior.
     */
    BasicElapsedTimer(ElapsedTimerState state = ElapsedTimerState::invalid)
    {
        if (state == ElapsedTimerState::started)
            restart();
    }

    void invalidate()
    {
        m_start = std::nullopt;
    }

    bool isValid() const
    {
        return static_cast<bool>(m_start);
    }

    Duration elapsed() const
    {
        if (!NX_ASSERT(isValid()))
            return Duration::zero();
        return std::chrono::duration_cast<Duration>(nx::utils::monotonicTime() - *m_start);
    }

    template<typename TargetDuration = Duration>
    std::optional<TargetDuration> maybeElapsed() const
    {
        if (!isValid())
            return std::nullopt;
        return std::chrono::duration_cast<TargetDuration>(nx::utils::monotonicTime() - *m_start);
    }

    bool hasExpired(Duration value) const
    {
        if (!isValid())
            return true;

        return elapsed() >= value;
    }

    /**
     * @return Elapsed time.
     */
    Duration restart()
    {
        const auto now = nx::utils::monotonicTime();
        const auto startBak = std::exchange(m_start, now);
        if (!startBak)
            return Duration::zero();
        return std::chrono::duration_cast<Duration>(now - *startBak);
    }

private:
    std::optional<std::chrono::steady_clock::time_point> m_start;
};

using ElapsedTimer = BasicElapsedTimer<std::chrono::milliseconds>;

class NX_UTILS_API SafeElapsedTimer: protected ElapsedTimer
{
    using base_type = ElapsedTimer;
public:
    SafeElapsedTimer(ElapsedTimerState state = ElapsedTimerState::invalid);

    bool hasExpired(std::chrono::milliseconds value) const;
    std::chrono::milliseconds restart();
    void invalidate();
    bool isValid() const;
    std::chrono::milliseconds elapsed() const;

    template<typename TargetDuration = std::chrono::milliseconds>
    std::optional<TargetDuration> maybeElapsed() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return base_type::maybeElapsed<TargetDuration>();
    }

private:
    mutable Mutex m_mutex;
};


} // namespace utils
} // namespace nx
