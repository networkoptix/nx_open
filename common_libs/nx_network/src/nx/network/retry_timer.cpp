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

const std::chrono::milliseconds RetryPolicy::kNoMaxDelay =
    std::chrono::milliseconds::zero();

const std::chrono::milliseconds RetryPolicy::kDefaultInitialDelay(500);
const std::chrono::milliseconds RetryPolicy::kDefaultMaxDelay(std::chrono::minutes(1));

RetryPolicy::RetryPolicy()
:
    m_maxRetryCount(kDefaultMaxRetryCount),
    m_initialDelay(kDefaultInitialDelay),
    m_delayMultiplier(kDefaultDelayMultiplier),
    m_maxDelay(kDefaultMaxDelay)
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

void RetryPolicy::setMaxDelay(std::chrono::milliseconds delay)
{
    m_maxDelay = delay;
}

std::chrono::milliseconds RetryPolicy::maxDelay() const
{
    return m_maxDelay;
}


////////////////////////////////////////////////////////////
//// class RetryTimer
////////////////////////////////////////////////////////////

RetryTimer::RetryTimer(const RetryPolicy& policy)
:
    m_retryPolicy(policy),
    m_currentDelay(m_retryPolicy.initialDelay()),
    m_effectiveMaxDelay(
        m_retryPolicy.maxDelay() == RetryPolicy::kNoMaxDelay
        ? std::chrono::milliseconds::max()
        : m_retryPolicy.maxDelay()),
    m_triesMade(0)
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

void RetryTimer::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.post(std::move(func));
}

void RetryTimer::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.dispatch(std::move(func));
}

bool RetryTimer::scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc)
{
    if (m_retryPolicy.maxRetryCount() != RetryPolicy::kInfiniteRetries &&
        m_triesMade >= m_retryPolicy.maxRetryCount())
    {
        return false;
    }

    if ((m_triesMade > 0) &&
        (m_retryPolicy.delayMultiplier() > 0) &&
        (m_currentDelay < m_effectiveMaxDelay))
    {
        //TODO #ak taking into account delay overflow
        m_currentDelay *= m_retryPolicy.delayMultiplier();
        if (m_currentDelay > m_effectiveMaxDelay)
            m_currentDelay = m_effectiveMaxDelay;
    }

    ++m_triesMade;
    m_timer.start(m_currentDelay, std::move(doAnotherTryFunc));
    return true;
}

std::chrono::milliseconds RetryTimer::currentDelay() const
{
    return m_currentDelay;
}

}   //network
}   //nx
