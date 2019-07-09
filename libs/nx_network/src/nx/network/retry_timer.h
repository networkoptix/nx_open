#pragma once

#include <chrono>
#include <limits>
#include <memory>

#include <nx/utils/progressive_delay_calculator.h>

#include "aio/basic_pollable.h"
#include "aio/timer.h"

namespace nx::network {

class NX_NETWORK_API RetryPolicy:
    public nx::utils::ProgressiveDelayPolicy
{
    using base_type = nx::utils::ProgressiveDelayPolicy;

public:
    constexpr static unsigned int kInfiniteRetries = std::numeric_limits<unsigned int>::max();
    constexpr static unsigned int kDefaultMaxRetryCount = 7;
    constexpr static double kDefaultRandomRatio = 0;

    const static RetryPolicy kNoRetries;

    unsigned int maxRetryCount;

    /**
     * Value from [0, 1], which defines random uniform dispersion for the delay. E.g. if
     * randomRatio=0.3, the resulting delay will be between 0.7 * X and 1.3 * X, where X is delay
     * calculated by ProgressiveDelayCalculator.
     */
    double randomRatio;

    RetryPolicy();
    RetryPolicy(
        unsigned int maxRetryCount,
        std::chrono::milliseconds initialDelay,
        unsigned int delayMultiplier,
        std::chrono::milliseconds maxDelay,
        double randomRatio);

    bool operator==(const RetryPolicy& rhs) const;

    QString toString() const;
};

class NX_NETWORK_API RetryDelayCalculator
{
public:
    RetryDelayCalculator(const RetryPolicy& retryPolicy);

    std::chrono::milliseconds calculateNewDelay();
    std::chrono::milliseconds currentDelay() const;
    unsigned int triesMade() const;
    unsigned int retriesLeft() const;
    void reset();

private:
    const RetryPolicy m_retryPolicy;

    nx::utils::ProgressiveDelayCalculator m_delayCalculator;
    double m_currentRandomBias;
};

/**
 * Implements request retry policy, supports policy specified in STUN rfc.
 * There are maximum N retries, delay between retries is increased by
 *   some multiplier with each unsuccessful try.
 * NOTE: RetryTimer instance can be safely freed within doAnotherTryFunc.
 * NOTE: Class methods are not thread-safe.
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
    RetryDelayCalculator m_delayCalculator;
    std::unique_ptr<aio::Timer> m_timer;
};

} // namespace nx::network
