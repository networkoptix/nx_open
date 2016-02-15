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
    std::function<void()> timerFunc)
{
    UDPSocket::registerTimer(timeout, std::move(timerFunc));
}

void Timer::post(std::function<void()> funcToCall)
{
    UDPSocket::post(std::move(funcToCall));
}

void Timer::dispatch(std::function<void()> funcToCall)
{
    UDPSocket::dispatch(std::move(funcToCall));
}

void Timer::cancelAsync(std::function<void()> completionHandler)
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
