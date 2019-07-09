#include "retry_timer.h"

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network {

//-------------------------------------------------------------------------------------------------
// class RetryPolicy

const RetryPolicy RetryPolicy::kNoRetries(
    0, kDefaultInitialDelay, kDefaultDelayMultiplier, kDefaultMaxDelay, kDefaultRandomRatio);

RetryPolicy::RetryPolicy():
    maxRetryCount(kDefaultMaxRetryCount),
    randomRatio(kDefaultRandomRatio)
{
}

RetryPolicy::RetryPolicy(
    unsigned int maxRetryCount,
    std::chrono::milliseconds initialDelay,
    unsigned int delayMultiplier,
    std::chrono::milliseconds maxDelay,
    double randomRatio)
:
    base_type(initialDelay, delayMultiplier, maxDelay),
    maxRetryCount(maxRetryCount),
    randomRatio(randomRatio)
{
    NX_ASSERT(randomRatio >= 0 && randomRatio <= 1);
}

bool RetryPolicy::operator==(const RetryPolicy& rhs) const
{
    return static_cast<const ProgressiveDelayPolicy&>(*this) == rhs
        && maxRetryCount == rhs.maxRetryCount
        && randomRatio == rhs.randomRatio;
}

QString RetryPolicy::toString() const
{
    return lm("RetryPolicy(%1, %2, %3, %4, %5)").args(
        maxRetryCount, initialDelay, delayMultiplier, maxDelay, randomRatio);
}

//-------------------------------------------------------------------------------------------------
// class RetryTimer

RetryTimer::RetryTimer(const RetryPolicy& policy, aio::AbstractAioThread* aioThread):
    aio::BasicPollable(aioThread),
    m_delayCalculator(policy),
    m_timer(std::make_unique<aio::Timer>(aioThread))
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

    m_timer->start(m_delayCalculator.calculateNewDelay(), std::move(doAnotherTryFunc));
    return true;
}

unsigned int RetryTimer::retriesLeft() const
{
    return m_delayCalculator.retriesLeft();
}

boost::optional<std::chrono::nanoseconds> RetryTimer::timeToEvent() const
{
    return m_timer->timeToEvent();
}

std::chrono::milliseconds RetryTimer::currentDelay() const
{
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

//-------------------------------------------------------------------------------------------------
// class RetryDelayCalculator

RetryDelayCalculator::RetryDelayCalculator(const RetryPolicy &retryPolicy)
    :
    m_retryPolicy(retryPolicy),
    m_delayCalculator(retryPolicy)
{
    reset();
}

std::chrono::milliseconds RetryDelayCalculator::calculateNewDelay()
{
    m_delayCalculator.calculateNewDelay();
    if (m_retryPolicy.randomRatio != 0)
        m_currentRandomBias = nx::utils::random::numberDelta(1.0, m_retryPolicy.randomRatio);

    return currentDelay();
}

std::chrono::milliseconds RetryDelayCalculator::currentDelay() const
{
    if (m_retryPolicy.randomRatio == 0)
        return m_delayCalculator.currentDelay();

    return std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(
        m_currentRandomBias * m_delayCalculator.currentDelay().count()));
}

unsigned int RetryDelayCalculator::triesMade() const
{
    return m_delayCalculator.triesMade();
}

unsigned int RetryDelayCalculator::retriesLeft() const
{
    if (m_retryPolicy.maxRetryCount == RetryPolicy::kInfiniteRetries)
        return RetryPolicy::kInfiniteRetries;

    return m_retryPolicy.maxRetryCount - m_delayCalculator.triesMade();
}

void RetryDelayCalculator::reset()
{
    m_delayCalculator.reset();
}

} // namespace nx::network
