/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/move_only_func.h>
#include <nx/utils/object_destruction_flag.h>

#include "aioeventhandler.h"
#include "basic_pollable.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Single-shot timer that runs in aio thread.
 */
class NX_NETWORK_API Timer:
    public BasicPollable,
    private AIOEventHandler<Pollable>
{
public:
    Timer(aio::AbstractAioThread* aioThread = nullptr);
    virtual ~Timer() override;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /** 
     * @note If timer is already started, this method overwrites timer, not adds a new one!
     */
    void start(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> timerFunc);
    std::chrono::nanoseconds timeToEvent() const;
    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);
    /**
     * Cancels timer waiting for timerFunc to complete.
     * Can be safely called within timer's aio thread.
     */
    void cancelSync();

private:
    nx::utils::MoveOnlyFunc<void()> m_handler;
    std::chrono::milliseconds m_timeout;
    std::chrono::steady_clock::time_point m_timerStartClock;
    AIOService& m_aioService;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
    int m_internalTimerId;

    virtual void stopWhileInAioThread() override;
    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override;
};

} // namespace aio
} // namespace network
} // namespace nx
