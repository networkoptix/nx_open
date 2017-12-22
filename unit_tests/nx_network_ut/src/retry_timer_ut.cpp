#include <gtest/gtest.h>

#include <nx/network/retry_timer.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {

TEST(RetryTimer, tryCount)
{
    const unsigned int retryCountValues[] = {0, 1, 10, RetryPolicy::kInfiniteRetries};

    for (const auto retryCount: retryCountValues)
    {
        RetryPolicy policy;
        policy.maxRetryCount = retryCount;
        policy.initialDelay = std::chrono::milliseconds(1);
        policy.maxDelay = std::chrono::milliseconds(1);

        RetryTimer retryTimer(policy);

        for (unsigned int i = 0;
            i < (retryCount == RetryPolicy::kInfiniteRetries ? 100 : retryCount);
            ++i)
        {
            nx::utils::promise<void> doNextTryCalledPromise;
            ASSERT_TRUE(retryTimer.scheduleNextTry(
                [&doNextTryCalledPromise] { doNextTryCalledPromise.set_value(); }));
            ASSERT_EQ(
                std::future_status::ready,
                doNextTryCalledPromise.get_future().wait_for(std::chrono::seconds(1)));
        }
        if (retryCount != RetryPolicy::kInfiniteRetries)
        {
            ASSERT_FALSE(retryTimer.scheduleNextTry([]{}));
        }

        retryTimer.pleaseStopSync();
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
    {
        for (const auto delayMultiplier: delayMultiplierValues)
        {
            RetryPolicy policy;
            policy.maxRetryCount = RetryPolicy::kInfiniteRetries;
            policy.delayMultiplier = delayMultiplier;
            policy.initialDelay = initialDelay;
            policy.maxDelay = maxDelay;

            RetryTimer retryTimer(policy);
            std::vector<std::chrono::milliseconds> delays;
            for (int i = 0; i < testLoopSize; ++i)
            {
                nx::utils::promise<void> doNextTryCalledPromise;
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

            retryTimer.pleaseStopSync();
        }
    }
}

} // namespace network
} // namespace nx
