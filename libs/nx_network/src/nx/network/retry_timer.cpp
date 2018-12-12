#include "retry_timer.h"

#include <nx/utils/std/cpp14.h>

namespace nx::network {

//-------------------------------------------------------------------------------------------------
// class RetryPolicy

const RetryPolicy RetryPolicy::kNoRetries(
    0, kDefaultInitialDelay, kDefaultDelayMultiplier, kDefaultMaxDelay);

RetryPolicy::RetryPolicy():
    maxRetryCount(kDefaultMaxRetryCount)
{
}

RetryPolicy::RetryPolicy(
    unsigned int maxRetryCount,
    std::chrono::milliseconds initialDelay,
    unsigned int delayMultiplier,
    std::chrono::milliseconds maxDelay)
:
    base_type(initialDelay, delayMultiplier, maxDelay),
    maxRetryCount(maxRetryCount)
{
}

bool RetryPolicy::operator==(const RetryPolicy& rhs) const
{
    return static_cast<const ProgressiveDelayPolicy&>(*this) == rhs
        && maxRetryCount == rhs.maxRetryCount;
}

//-------------------------------------------------------------------------------------------------
// class RetryTimer

RetryTimer::RetryTimer(const RetryPolicy& policy, aio::AbstractAioThread* aioThread):
    aio::BasicPollable(aioThread),
    m_delayCalculator(policy),
    m_timer(std::make_unique<aio::Timer>(aioThread)),
    m_retryPolicy(policy)
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

    m_timer->start(
        m_delayCalculator.calculateNewDelay(),
        std::move(doAnotherTryFunc));
    return true;
}

unsigned int RetryTimer::retriesLeft() const
{
    if (m_retryPolicy.maxRetryCount == RetryPolicy::kInfiniteRetries)
        return RetryPolicy::kInfiniteRetries;

    return m_retryPolicy.maxRetryCount - m_delayCalculator.triesMade();
}

boost::optional<std::chrono::nanoseconds> RetryTimer::timeToEvent() const
{
    return m_timer->timeToEvent();
}

std::chrono::milliseconds RetryTimer::currentDelay() const
{
    //return m_currentDelay;
    return m_delayCalculator.currentDelay();
}

void RetryTimer::reset()
{
    m_delayCalculator.reset();
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

} // namespace nx::network
