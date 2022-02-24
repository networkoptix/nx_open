// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
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

    const static RetryPolicy kNoRetries;

    unsigned int maxRetryCount;

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

/**
 * Timer that can be used to retry some operation with increasing delay and a limited number of retries.
 * Implements requested retry policy, supports policy specified in STUN rfc.
 * There are maximum N retries, delay between retries is increased by some multiplier with each
 * unsuccessful try.
 *
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
     * @return false, if maximum retries have been done.
     */
    bool scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc);

    unsigned int retriesLeft() const;
    std::optional<std::chrono::nanoseconds> timeToEvent() const;

    /**
     * @return current delay between retries.
     */
    std::chrono::milliseconds currentDelay() const;

    /**
     * Resets internal state to default values.
     * NOTE: Does not cancel I/O.
     */
    void reset();

    void cancelAsync(nx::utils::MoveOnlyFunc<void()> completionHandler);
    void cancelSync();

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::utils::ProgressiveDelayCalculator m_delayCalculator;
    std::unique_ptr<aio::Timer> m_timer;
    const RetryPolicy m_retryPolicy;
};

} // namespace nx::network
