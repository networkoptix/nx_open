/**********************************************************
* Mar 24, 2016
* akolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>

#include <nx/network/retry_timer.h>


namespace nx {
namespace network {

TEST(RetryTimer, tryCount)
{
    const unsigned int retryCountValues[] = {0, 1, 10, RetryPolicy::kInfiniteRetries};

    for (const auto retryCount: retryCountValues)
    {
        RetryPolicy policy;
        policy.setMaxRetryCount(retryCount);
        policy.setInitialDelay(std::chrono::milliseconds(1));
        policy.setMaxDelay(std::chrono::milliseconds(1));

        RetryTimer retryTimer(policy);

        for (unsigned int i = 0;
            i < (retryCount == RetryPolicy::kInfiniteRetries ? 100 : retryCount);
            ++i)
        {
            std::promise<void> doNextTryCalledPromise;
            ASSERT_TRUE(retryTimer.scheduleNextTry(
                [&doNextTryCalledPromise] { doNextTryCalledPromise.set_value(); }));
            ASSERT_EQ(
                std::future_status::ready,
                doNextTryCalledPromise.get_future().wait_for(std::chrono::seconds(1)));
        }
        if (retryCount != RetryPolicy::kInfiniteRetries)
            ASSERT_FALSE(retryTimer.scheduleNextTry([]{}));
    }
}

TEST(RetryTimer, delayCalculation)
{
    const unsigned int delayMultiplierValues[] = { 0, 1, 2 };
    const std::chrono::milliseconds maxDelayValues[] = 
        { std::chrono::milliseconds::zero(), std::chrono::milliseconds(350), std::chrono::seconds(10) };
    const std::chrono::milliseconds initialDelay(1);
    const int testLoopSize = 10;

    for (const auto maxDelay: maxDelayValues)
        for (const auto delayMultiplier : delayMultiplierValues)
        {
            RetryPolicy policy;
            policy.setMaxRetryCount(RetryPolicy::kInfiniteRetries);
            policy.setDelayMultiplier(delayMultiplier);
            policy.setInitialDelay(initialDelay);
            policy.setMaxDelay(maxDelay);

            RetryTimer retryTimer(policy);
            std::vector<std::chrono::milliseconds> delays;
            for (int i = 0; i < testLoopSize; ++i)
            {
                std::promise<void> doNextTryCalledPromise;
                ASSERT_TRUE(retryTimer.scheduleNextTry(
                    [&doNextTryCalledPromise] { doNextTryCalledPromise.set_value(); }));
                delays.push_back(retryTimer.currentDelay());
                doNextTryCalledPromise.get_future().wait();
            }

            boost::optional<std::chrono::milliseconds> prevDelay;
            for (const auto& delay: delays)
            {
                std::chrono::milliseconds expectedDelay =
                    prevDelay
                    ? (prevDelay.get() * (delayMultiplier == 0 ? 1 : delayMultiplier))
                    : initialDelay;
                if ((maxDelay != RetryPolicy::kNoMaxDelay) && (expectedDelay > maxDelay))
                    expectedDelay = maxDelay;
                ASSERT_EQ(expectedDelay, delay);
                prevDelay = delay;
            }
        }
}

}   //network
}   //nx
