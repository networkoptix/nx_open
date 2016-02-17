/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#include "timer.h"


namespace nx {
namespace network {
namespace aio {

void Timer::start(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> timerFunc)
{
    UDPSocket::registerTimer(timeout, std::move(timerFunc));
    m_timeout = timeout;
    m_timerStartClock = std::chrono::steady_clock::now();
}

std::chrono::nanoseconds Timer::timeToEvent() const
{
    const auto elapsed = std::chrono::steady_clock::now() - m_timerStartClock;
    return elapsed >= m_timeout
        ? std::chrono::nanoseconds::zero()
        : m_timeout - elapsed;
}

void Timer::post(nx::utils::MoveOnlyFunc<void()> funcToCall)
{
    UDPSocket::post(std::move(funcToCall));
}

void Timer::dispatch(nx::utils::MoveOnlyFunc<void()> funcToCall)
{
    UDPSocket::dispatch(std::move(funcToCall));
}

void Timer::cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    UDPSocket::cancelIOAsync(etTimedOut, std::move(completionHandler));
}

void Timer::cancelSync()
{
    UDPSocket::cancelIOSync(etTimedOut);
}

AbstractAioThread* Timer::getAioThread()
{
    return UDPSocket::getAioThread();
}

void Timer::bindToAioThread(AbstractAioThread* aioThread)
{
    UDPSocket::bindToAioThread(aioThread);
}

}   //aio
}   //network
}   //nx
