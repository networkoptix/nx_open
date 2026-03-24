// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include <nx/utils/log/log.h>

namespace nx::utils {

/**
 * RAII watchdog: fires a callback if the guarded scope does not complete
 * within the given timeout.  Destruction cancels the watchdog.
 *
 * Typical usage:
 *   ScopedTimeoutCallback watchdog(
 *       []() { cancelPendingIo(); },
 *       std::chrono::seconds(10));
 *   // ... long operation ...
 *   // ~watchdog cancels the timer if the operation finishes in time.
 */
class NX_UTILS_API ScopedTimeoutCallback
{
public:
    ScopedTimeoutCallback(
        std::function<void()> onTimeout,
        std::chrono::milliseconds timeout);
    ~ScopedTimeoutCallback();

    ScopedTimeoutCallback(const ScopedTimeoutCallback&) = delete;
    ScopedTimeoutCallback& operator=(const ScopedTimeoutCallback&) = delete;

private:
    void run();

    std::function<void()> m_onTimeout;
    std::chrono::milliseconds m_timeout;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_done = false;
    std::thread m_thread;
};

} // namespace nx::utils
