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

constexpr const std::chrono::milliseconds RetryPolicy::kNoMaxDelay;
constexpr const std::chrono::milliseconds RetryPolicy::kDefaultInitialDelay;
constexpr const std::chrono::milliseconds RetryPolicy::kDefaultMaxDelay;

RetryPolicy::RetryPolicy()
:
    m_maxRetryCount(kDefaultMaxRetryCount),
    m_initialDelay(kDefaultInitialDelay),
    m_delayMultiplier(kDefaultDelayMultiplier),
    m_maxDelay(kDefaultMaxDelay)
{
}

RetryPolicy::RetryPolicy(
    unsigned int maxRetryCount,
    std::chrono::milliseconds initialDelay,
    unsigned int delayMultiplier,
    std::chrono::milliseconds maxDelay)
:
    m_maxRetryCount(maxRetryCount),
    m_initialDelay(initialDelay),
    m_delayMultiplier(delayMultiplier),
    m_maxDelay(maxDelay)
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
    m_triesMade(0)
{
    reset();
}

RetryTimer::~RetryTimer()
{
}

void RetryTimer::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer.pleaseStop(std::move(completionHandler));
}

void RetryTimer::pleaseStopSync(bool doNotCheckForLocks)
{
    m_timer.pleaseStopSync(doNotCheckForLocks);
}

aio::AbstractAioThread* RetryTimer::getAioThread() const
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
        auto newDelay = m_currentDelay * m_retryPolicy.delayMultiplier();
        if (newDelay == std::chrono::milliseconds::zero())
            newDelay = std::chrono::milliseconds(1);
        if (newDelay < m_currentDelay)
        {
            //overflow
            newDelay = std::chrono::milliseconds::max();
        }
        m_currentDelay = newDelay;
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

void RetryTimer::reset()
{
    m_currentDelay = m_retryPolicy.initialDelay();
    m_effectiveMaxDelay = 
        m_retryPolicy.maxDelay() == RetryPolicy::kNoMaxDelay
        ? std::chrono::milliseconds::max()
        : m_retryPolicy.maxDelay();
    m_triesMade = 0;
}

}   //network
}   //nx
