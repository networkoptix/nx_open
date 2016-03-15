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
    static const unsigned int kInfiniteRetries = 
        std::numeric_limits<unsigned int>::max();
    static const unsigned int kDefaultMaxRetryCount = 7;
    /** 500ms */
    static const std::chrono::milliseconds kDefaultInitialDelay;
    static const unsigned int kDefaultDelayMultiplier = 2;

    RetryPolicy();

    void setMaxRetryCount(unsigned int retryCount);
    unsigned int maxRetryCount() const;

    void setInitialDelay(std::chrono::milliseconds delay);
    std::chrono::milliseconds initialDelay() const;

    /** Value of 0 means no multiplier (actually, same as 1) */
    void setDelayMultiplier(unsigned int multiplier);
    unsigned int delayMultiplier() const;

private:
    unsigned int m_maxRetryCount;
    std::chrono::milliseconds m_initialDelay;
    unsigned int m_delayMultiplier;
};

/** Implements request retry policy, specified in STUN rfc.
    There are maximum N retries, delay between retries is increased by 
        some multiplier with each unsuccessful try.
    \note Object of this class can be used for a single retry sequence only. 
        No "reset state" is provided!
    \note \a RetryTimer instance can be safely freed within \a doAnotherTryFunc
*/
class NX_NETWORK_API RetryTimer
:
    public QnStoppableAsync
{
public:
    RetryTimer(const RetryPolicy& policy);
    virtual ~RetryTimer();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync() override;

    /** TODO #ak introduce abstract class for the next two methods */
    aio::AbstractAioThread* getAioThread();
    void bindToAioThread(aio::AbstractAioThread* aioThread);

    /** Returns \a false, if maximum retries have been done */
    bool scheduleNextTry(nx::utils::MoveOnlyFunc<void()> doAnotherTryFunc);

private:
    aio::Timer m_timer;
    const RetryPolicy m_retryPolicy;
};

}   //network
}   //nx
