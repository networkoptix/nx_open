/**********************************************************
* Mar 15, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <limits>

#include <utils/common/stoppable.h>

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

    RetryPolicy();

    void setMaxRetryCount(unsigned int retryCount);
    unsigned int maxRetryCount() const;

    void setInitialDelay(std::chrono::milliseconds delay);
    std::chrono::milliseconds initialDelay() const;

    /** Value of 0 means no multiplier (actually, same as 1) */
    void setDelayMultiplier(unsigned int multiplier);
    unsigned int delayMultiplier() const;

    /** \a std::chrono::milliseconds::zero is treated as no limit */
    void setMaxDelay(std::chrono::milliseconds delay);
    std::chrono::milliseconds maxDelay() const;

private:
    unsigned int m_maxRetryCount;
    std::chrono::milliseconds m_initialDelay;
    unsigned int m_delayMultiplier;
    std::chrono::milliseconds m_maxDelay;
};

namespace aio {

/** TODO #ak deal with Pollable and AbstractPollable */
class AbstractPollable
{
public:
    virtual ~AbstractPollable() {}

    virtual aio::AbstractAioThread* getAioThread() = 0;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) = 0;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) = 0;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) = 0;
};

}   //aio

/** Implements request retry policy, specified in STUN rfc.
    There are maximum N retries, delay between retries is increased by 
        some multiplier with each unsuccessful try.
    \note Object of this class can be used for a single retry sequence only. 
        No "reset state" is provided!
    \note \a RetryTimer instance can be safely freed within \a doAnotherTryFunc
*/
class NX_NETWORK_API RetryTimer
:
    public aio::AbstractPollable,
    public QnStoppableAsync
{
public:
    RetryTimer(const RetryPolicy& policy);
    virtual ~RetryTimer();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync() override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    /** Returns \a false, if maximum retries have been done */
    bool scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc);

    std::chrono::milliseconds currentDelay() const;

private:
    aio::Timer m_timer;
    const RetryPolicy m_retryPolicy;
    std::chrono::milliseconds m_currentDelay;
    std::chrono::milliseconds m_effectiveMaxDelay;
    unsigned int m_triesMade;
};

}   //network
}   //nx
