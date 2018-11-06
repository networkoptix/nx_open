#pragma once

#include <chrono>
#include <shared_mutex>
#include <nx/utils/thread/mutex.h>

#include <QtCore/QElapsedTimer>

namespace nx {
namespace utils {

class NX_UTILS_API ElapsedTimerThreadSafe
{
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

private:
    mutable QnReadWriteLock m_mutex;
    QElapsedTimer m_timer;
};

} // namespace utils
} // namespace nx
