/**********************************************************
* Feb 12, 2016
* akolesnikov
***********************************************************/

#include "timer.h"


namespace nx {
namespace network {
namespace aio {

void Timer::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_internalSocket.pleaseStop(std::move(completionHandler));
}

void Timer::pleaseStopSync()
{
    m_internalSocket.pleaseStopSync();
}

void Timer::post(nx::utils::MoveOnlyFunc<void()> funcToCall)
{
    m_internalSocket.post(std::move(funcToCall));
}

void Timer::dispatch(nx::utils::MoveOnlyFunc<void()> funcToCall)
{
    m_internalSocket.dispatch(std::move(funcToCall));
}

AbstractAioThread* Timer::getAioThread() const
{
    return m_internalSocket.getAioThread();
}

void Timer::bindToAioThread(AbstractAioThread* aioThread)
{
    m_internalSocket.bindToAioThread(aioThread);
}

void Timer::start(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void()> timerFunc)
{
    //TODO #ak UDPSocket::registerTimer currently does not support zero timeouts, 
        //so using following hack
    if (timeout == std::chrono::milliseconds::zero())
        timeout = std::chrono::milliseconds(1); 

    m_internalSocket.registerTimer(timeout, std::move(timerFunc));
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

void Timer::cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_internalSocket.cancelIOAsync(etTimedOut, std::move(completionHandler));
}

void Timer::cancelSync()
{
    m_internalSocket.cancelIOSync(etTimedOut);
}

bool Timer::inSelfAioThread() const
{
    return m_internalSocket.inSelfAioThread();
}

}   //aio
}   //network
}   //nx
