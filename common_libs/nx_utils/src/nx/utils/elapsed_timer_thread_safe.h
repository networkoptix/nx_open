#pragma once

#include <chrono>
#include <shared_mutex>

#include <QtCore/QElapsedTimer>

namespace nx {
namespace utils {

class NX_UTILS_API ElapsedTimerThreadSafe
{
    // TODO: change to shared_timed_mutex to shared_mutex when c++17 available
    using mutex_type = std::shared_timed_mutex;
    mutable mutex_type m_mutex;
    QElapsedTimer m_timer;

public:
    void start();
    void stop();

    /** @return 0 if timer is stopped when function is invoked */
    std::chrono::milliseconds elapsedSinceStart() const;

    /** @return false if timer was is stopped when function is invoked */
    bool hasExpiredSinceStart(std::chrono::milliseconds ms) const;

    bool isStarted() const;
    std::chrono::milliseconds elapsed() const;
    bool hasExpired(std::chrono::milliseconds ms) const;
};

} // namespace utils
} // namespace nx
