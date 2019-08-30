#pragma once

#include <chrono>
#include <optional>

namespace nx {
namespace utils {

enum class ElapsedTimerState
{
    invalid,
    started,
};

/**
 * Calculates elapsed time using std::chrono::steady_clock.
 * @param Duration std::chrono duration type.
 * NOTE: isValid() returns false just after construction. So, restart() has to be invoked.
 * @param Duration std::chrono duration type.
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
        return std::chrono::duration_cast<Duration>(
            std::chrono::steady_clock::now() - *m_start);
    }

    bool hasExpired(std::chrono::milliseconds value) const
    {
        return elapsed() >= value;
    }

    /**
     * @return Elapsed time.
     */
    Duration restart()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto startBak = std::exchange(m_start, now);
        if (startBak)
            return std::chrono::duration_cast<Duration>(now - *startBak);
        else
            return Duration::zero();
    }

private:
    std::optional<std::chrono::steady_clock::time_point> m_start;
};

using ElapsedTimer = BasicElapsedTimer<std::chrono::milliseconds>;

} // namespace utils
} // namespace nx
