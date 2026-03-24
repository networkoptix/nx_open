// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scoped_timeout_callback.h"

namespace nx::utils {

ScopedTimeoutCallback::ScopedTimeoutCallback(
    std::function<void()> onTimeout,
    std::chrono::milliseconds timeout)
    :
    m_onTimeout(std::move(onTimeout)),
    m_timeout(timeout),
    m_thread([this]() { run(); })
{
}

ScopedTimeoutCallback::~ScopedTimeoutCallback()
{
    {
        std::lock_guard lock(m_mutex);
        m_done = true;
    }
    m_cv.notify_one();
    m_thread.join();
}

void ScopedTimeoutCallback::run()
{
    std::unique_lock lock(m_mutex);
    if (!m_cv.wait_for(lock, m_timeout, [this]() { return m_done; }))
        m_onTimeout();
}

} // namespace nx::utils
