#include "retry_timer.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {

//-------------------------------------------------------------------------------------------------
// class RetryPolicy

constexpr const std::chrono::milliseconds RetryPolicy::kNoMaxDelay;
constexpr const std::chrono::milliseconds RetryPolicy::kDefaultInitialDelay;
constexpr const std::chrono::milliseconds RetryPolicy::kDefaultMaxDelay;

const RetryPolicy RetryPolicy::kNoRetries(
    0, kDefaultInitialDelay, kDefaultDelayMultiplier, kDefaultMaxDelay);

RetryPolicy::RetryPolicy():
    maxRetryCount(kDefaultMaxRetryCount),
    initialDelay(kDefaultInitialDelay),
    delayMultiplier(kDefaultDelayMultiplier),
    maxDelay(kDefaultMaxDelay)
{
}

RetryPolicy::RetryPolicy(
    unsigned int maxRetryCount,
    std::chrono::milliseconds initialDelay,
    unsigned int delayMultiplier,
    std::chrono::milliseconds maxDelay)
:
    maxRetryCount(maxRetryCount),
    initialDelay(initialDelay),
    delayMultiplier(delayMultiplier),
    maxDelay(maxDelay)
{
}

bool RetryPolicy::operator==(const RetryPolicy& rhs) const
{
    return maxRetryCount == rhs.maxRetryCount
        && initialDelay == rhs.initialDelay
        && delayMultiplier == rhs.delayMultiplier
        && maxDelay == rhs.maxDelay;
}


//-------------------------------------------------------------------------------------------------
// class RetryTimer

RetryTimer::RetryTimer(const RetryPolicy& policy, aio::AbstractAioThread* aioThread):
    aio::BasicPollable(aioThread),
    m_timer(std::make_unique<aio::Timer>(aioThread)),
    m_retryPolicy(policy),
    m_triesMade(0)
{
    bindToAioThread(getAioThread());
    reset();
}

RetryTimer::~RetryTimer()
{
    stopWhileInAioThread();
}

void RetryTimer::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    aio::BasicPollable::bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);
}

bool RetryTimer::scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc)
{
    if (retriesLeft() == 0)
        return false;

    if ((m_triesMade > 0) &&
        (m_retryPolicy.delayMultiplier > 0) &&
        (m_currentDelay < m_effectiveMaxDelay))
    {
        auto newDelay = m_currentDelay * m_retryPolicy.delayMultiplier;
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
    m_timer->start(m_currentDelay, std::move(doAnotherTryFunc));
    return true;
}

unsigned int RetryTimer::retriesLeft() const
{
    if (m_retryPolicy.maxRetryCount == RetryPolicy::kInfiniteRetries)
        return RetryPolicy::kInfiniteRetries;

    return m_retryPolicy.maxRetryCount - m_triesMade;
}

boost::optional<std::chrono::nanoseconds> RetryTimer::timeToEvent() const
{
    return m_timer->timeToEvent();
}

std::chrono::milliseconds RetryTimer::currentDelay() const
{
    return m_currentDelay;
}

void RetryTimer::reset()
{
    m_currentDelay = m_retryPolicy.initialDelay;
    m_effectiveMaxDelay =
        m_retryPolicy.maxDelay == RetryPolicy::kNoMaxDelay
        ? std::chrono::milliseconds::max()
        : m_retryPolicy.maxDelay;
    m_triesMade = 0;
}

void RetryTimer::cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer->cancelAsync(
        [this, completionHandler = std::move(completionHandler)]()
        {
            reset();
            completionHandler();
        });
}

void RetryTimer::cancelSync()
{
    m_timer->cancelSync();
    reset();
}

void RetryTimer::stopWhileInAioThread()
{
    m_timer.reset();
}

} // namespace network
} // namespace nx
