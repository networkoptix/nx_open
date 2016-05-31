/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/move_only_func.h>

#include "abstract_pollable.h"
#include "nx/network/system_socket.h"


namespace nx {
namespace network {
namespace aio {

/**
 * Single-shot timer that runs in aio thread.
 //TODO #ak introduce implementation without socket handle
 */
class NX_NETWORK_API Timer
:
    public AbstractPollable
{
public:
    Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync() override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> funcToCall) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> funcToCall) override;
    virtual AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    /** 
        \note If timer is already started, this method overwrites timer, not adds a new one!
    */
    void start(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> timerFunc);
    std::chrono::nanoseconds timeToEvent() const;
    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);
    /** Cancels timer waiting for \a timerFunc to complete.
        Can be safely called within timer's aio thread
    */
    void cancelSync();

    bool isInSelfAioThread() const;

private:
    std::chrono::milliseconds m_timeout;
    std::chrono::steady_clock::time_point m_timerStartClock;
    UDPSocket m_internalSocket;
};

}   //aio
}   //network
}   //nx
