#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/object_destruction_flag.h>

#include "aio_event_handler.h"
#include "basic_pollable.h"

namespace nx::network::aio {

using TimerEventHandler = nx::utils::MoveOnlyFunc<void()>;

/**
 * Single-shot timer that runs in aio thread.
 */
class NX_NETWORK_API Timer:
    public BasicPollable,
    private AIOEventHandler
{
public:
    Timer(aio::AbstractAioThread* aioThread = nullptr);
    virtual ~Timer() override;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /**
     * NOTE: If timer is already started, this method overwrites timer, not adds a new one!
     */
    void start(
        std::chrono::milliseconds timeout,
        TimerEventHandler timerFunc);

    /**
     * Future-based wrapper for start.
     *
     * @throws std::system_error with std::errc::operation_canceled error code when operation is
     * canceled.
     */
    cf::future<cf::unit> start(std::chrono::milliseconds timeout);

    boost::optional<std::chrono::nanoseconds> timeToEvent() const;

    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);

    /**
     * Future-based wrapper for cancelAsync.
     */
    cf::future<cf::unit> cancel();

    /**
     * Cancels timer waiting for timerFunc to complete.
     * Can be safely called within timer's aio thread.
     * Can be safely called within non aio thread, only if called not under the same mutex, that is used
     * in callback function.
     */
    void cancelSync();

protected:
    virtual void stopWhileInAioThread() override;
    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override;

private:
    TimerEventHandler m_handler;
    std::chrono::milliseconds m_timeout;
    boost::optional<std::chrono::steady_clock::time_point> m_timerStartClock;
    AIOService& m_aioService;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
    int m_internalTimerId;
};

} // namespace nx::network::aio
