/**********************************************************
* Mar 15, 2016
* akolesnikov
***********************************************************/

#include "retry_timer.h"


namespace nx {
namespace network {

////////////////////////////////////////////////////////////
//// class RetryPolicy
////////////////////////////////////////////////////////////

const std::chrono::milliseconds RetryPolicy::kDefaultInitialDelay(500);

RetryPolicy::RetryPolicy()
:
    m_maxRetryCount(kDefaultMaxRetryCount),
    m_initialDelay(kDefaultInitialDelay),
    m_delayMultiplier(kDefaultDelayMultiplier)
{
}

void RetryPolicy::setMaxRetryCount(unsigned int retryCount)
{
    m_maxRetryCount = retryCount;
}

unsigned int RetryPolicy::maxRetryCount() const
{
    return m_maxRetryCount;
}

void RetryPolicy::setInitialDelay(std::chrono::milliseconds delay)
{
    m_initialDelay = delay;
}

std::chrono::milliseconds RetryPolicy::initialDelay() const
{
    return m_initialDelay;
}

/** Value of 0 means no multiplier (actually, same as 1) */
void RetryPolicy::setDelayMultiplier(unsigned int multiplier)
{
    m_delayMultiplier = multiplier;
}

unsigned int RetryPolicy::delayMultiplier() const
{
    return m_delayMultiplier;
}


////////////////////////////////////////////////////////////
//// class RetryTimer
////////////////////////////////////////////////////////////

RetryTimer::RetryTimer(const RetryPolicy& policy)
:
    m_retryPolicy(policy)
{
}

RetryTimer::~RetryTimer()
{
}

void RetryTimer::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer.cancelAsync(std::move(completionHandler));
}

void RetryTimer::pleaseStopSync()
{
    m_timer.pleaseStopSync();
}

aio::AbstractAioThread* RetryTimer::getAioThread()
{
    return m_timer.getAioThread();
}

void RetryTimer::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
}

bool RetryTimer::scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc)
{
    //TODO #ak
    return false;
}

}   //network
}   //nx
