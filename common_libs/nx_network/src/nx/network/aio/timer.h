/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/move_only_func.h>

#include "nx/network/system_socket.h"


namespace nx {
namespace network {
namespace aio {

/**
 * Single-shot timer that runs in aio thread.
 */
class NX_NETWORK_API Timer
:
    private UDPSocket   //TODO #ak introduce implementation without socket handle
{
public:
    using UDPSocket::pleaseStop;
    using UDPSocket::pleaseStopSync;

    Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    /** 
        \note If timer is already started, this method overwrites timer, not adds a new one!
    */
    void start(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> timerFunc);
    std::chrono::nanoseconds timeToEvent() const;
    void post(nx::utils::MoveOnlyFunc<void()> funcToCall);
    void dispatch(nx::utils::MoveOnlyFunc<void()> funcToCall);
    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);
    /** Cancels timer waiting for \a timerFunc to complete.
        Can be safely called within timer's aio thread
    */
    void cancelSync();

    AbstractAioThread* getAioThread() const;
    void bindToAioThread(AbstractAioThread* aioThread);

private:
    std::chrono::milliseconds m_timeout;
    std::chrono::steady_clock::time_point m_timerStartClock;
};

}   //aio
}   //network
}   //nx
