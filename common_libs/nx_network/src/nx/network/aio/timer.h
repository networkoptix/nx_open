/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <functional>

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

    void start(
        std::chrono::milliseconds timeout,
        std::function<void()> timerFunc);
    std::chrono::nanoseconds timeToEvent() const;
    void post(std::function<void()> funcToCall);
    void dispatch(std::function<void()> funcToCall);
    void cancelAsync(std::function<void()> completionHandler);
    /** Cancels timer waiting for \a timerFunc to complete.
        Can be safely called within timer's aio thread
    */
    void cancelSync();

    AbstractAioThread* getAioThread();
    void bindToAioThread(AbstractAioThread* aioThread);

private:
    std::chrono::milliseconds m_timeout;
    std::chrono::steady_clock::time_point m_timerStartClock;
};

}   //aio
}   //network
}   //nx
