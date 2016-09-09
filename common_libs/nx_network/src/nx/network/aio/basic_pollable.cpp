/**********************************************************
* Jun 30, 2016
* akolesnikov
***********************************************************/

#include "basic_pollable.h"

#include <nx/utils/std/future.h>


namespace nx {
namespace network {
namespace aio {

BasicPollable::BasicPollable(aio::AbstractAioThread* aioThread)
{
    if (aioThread)
        m_timer.bindToAioThread(aioThread);
}

void BasicPollable::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]
        {
            stopWhileInAioThread();
            completionHandler();
        });
}

void BasicPollable::pleaseStopSync(bool doNotCheckForLocks)
{
    if (m_timer.isInSelfAioThread())
        stopWhileInAioThread();
    else
        QnStoppableAsync::pleaseStopSync(doNotCheckForLocks);
}

aio::AbstractAioThread* BasicPollable::getAioThread() const
{
    return m_timer.getAioThread();
}

void BasicPollable::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
}

void BasicPollable::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.post(std::move(func));
}

void BasicPollable::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.dispatch(std::move(func));
}

Timer* BasicPollable::timer()
{
    return &m_timer;
}

}   //namespace aio
}   //namespace network
}   //namespace nx
