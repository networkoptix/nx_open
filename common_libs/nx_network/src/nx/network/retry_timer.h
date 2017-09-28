#pragma once

#include <chrono>
#include <limits>
#include <memory>

#include "aio/basic_pollable.h"
#include "aio/timer.h"

namespace nx {
namespace network {

class NX_NETWORK_API RetryPolicy
{
public:
    constexpr static const unsigned int kInfiniteRetries =
        std::numeric_limits<unsigned int>::max();
    constexpr static const std::chrono::milliseconds kNoMaxDelay =
        std::chrono::milliseconds::zero();

    constexpr static const unsigned int kDefaultMaxRetryCount = 7;
    constexpr static const std::chrono::milliseconds kDefaultInitialDelay =
        std::chrono::milliseconds(500);
    constexpr static const unsigned int kDefaultDelayMultiplier = 2;
    constexpr static const std::chrono::milliseconds kDefaultMaxDelay =
        std::chrono::minutes(1);

    const static RetryPolicy kNoRetries;

    unsigned int maxRetryCount;
    std::chrono::milliseconds initialDelay;
    /**
     * Value of 0 means no multiplier (actually, same as 1).
     */
    unsigned int delayMultiplier;
    /**
     * std::chrono::milliseconds::zero is treated as no limit.
     */
    std::chrono::milliseconds maxDelay;

    RetryPolicy();
    RetryPolicy(
        unsigned int maxRetryCount,
        std::chrono::milliseconds initialDelay,
        unsigned int delayMultiplier,
        std::chrono::milliseconds maxDelay);

    bool operator==(const RetryPolicy& rhs) const;
};

/**
 * Implements request retry policy, specified in STUN rfc.
 * There are maximum N retries, delay between retries is increased by 
 *   some multiplier with each unsuccessful try.
 * @note RetryTimer instance can be safely freed within doAnotherTryFunc.
 * @note Class methods are not thread-safe.
 */
class NX_NETWORK_API RetryTimer:
    public aio::BasicPollable
{
public:
    RetryTimer(const RetryPolicy& policy, aio::AbstractAioThread* aioThread = nullptr);
    virtual ~RetryTimer();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Returns false, if maximum retries have been done.
     */
    bool scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc);

    unsigned int retriesLeft() const;
    boost::optional<std::chrono::nanoseconds> timeToEvent() const;
    std::chrono::milliseconds currentDelay() const;

    /**
     * Resets internal state to default values.
     */
    void reset();

    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);
    void cancelSync();

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<aio::Timer> m_timer;
    const RetryPolicy m_retryPolicy;
    std::chrono::milliseconds m_currentDelay;
    std::chrono::milliseconds m_effectiveMaxDelay;
    unsigned int m_triesMade;
};

}   //network
}   //nx
