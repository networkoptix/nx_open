#pragma once

#include <chrono>
#include <optional>

namespace nx {
namespace utils {

/**
 * Calculates elapsed time using std::chrono::steady_clock.
 * @param Duration std::chrono duration type.
 * NOTE: isValid() returns false just after construction. So, restart() has to be invoked.
 */
template<typename Duration = std::chrono::milliseconds>
class BasicElapsedTimer
{
public:
    BasicElapsedTimer() = default;

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
        {
            return std::chrono::duration_cast<Duration>(
                std::chrono::steady_clock::now() - *startBak);
        }
        else
        {
            return Duration::zero();
        }
    }

private:
    std::optional<std::chrono::steady_clock::time_point> m_start;
};

using ElapsedTimer = BasicElapsedTimer<std::chrono::milliseconds>;

} // namespace utils
} // namespace nx
