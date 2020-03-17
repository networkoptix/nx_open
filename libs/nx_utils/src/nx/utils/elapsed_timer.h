#pragma once

#include <optional>
#include <chrono>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

/**
 * It is almost the same as the QElapsedTimer, but implemented using nx::utils::monotonicTime().
 * This implementation allows writing efficient unit-tests for classes using this timer.
 * Additionally, it never produces UB unlike original QElapsedTimer.
 *
 * NOTE: It is not thread-safe.
 */
class NX_UTILS_API ElapsedTimer
{
public:
    ElapsedTimer(bool started = false);

    bool hasExpired(std::chrono::milliseconds value) const;
    std::chrono::milliseconds restart();
    void invalidate();
    bool isValid() const;
    std::chrono::milliseconds elapsed() const;
    int64_t elapsedMs() const;
private:

    std::optional<std::chrono::steady_clock::time_point> m_lastRestartTime;
};

class NX_UTILS_API SafeElapsedTimer: protected ElapsedTimer
{
    using base_type = ElapsedTimer;
public:
    SafeElapsedTimer(bool started = false);

    bool hasExpired(std::chrono::milliseconds value) const;
    std::chrono::milliseconds restart();
    void invalidate();
    bool isValid() const;
    std::chrono::milliseconds elapsed() const;
    int64_t elapsedMs() const;
private:
    mutable Mutex m_mutex;
};

} // namespace utils
} // namespace nx
