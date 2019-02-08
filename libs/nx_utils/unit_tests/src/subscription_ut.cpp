#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include <nx/utils/std/thread.h>
#include <nx/utils/subscription.h>

TEST(UtilsCommon, SubscriptionGuard)
{
    nx::utils::Subscription<int> subscription;

    int a = 1;
    nx::utils::SubscriptionId subscription1 = nx::utils::kInvalidSubscriptionId;
    subscription.subscribe(
        [&](int v) { a = v; },
        &subscription1);

    subscription.notify(2);
    EXPECT_EQ(a, 2);

    int b = 1;
    {
        nx::utils::SubscriptionId subscription2 = nx::utils::kInvalidSubscriptionId;
        subscription.subscribe(
            [&](int v) { b = v; },
            &subscription2);

        subscription.notify(3);
        EXPECT_EQ(a, 3);
        EXPECT_EQ(b, 3);

        subscription.removeSubscription(subscription2);
    }

    subscription.notify(4);
    EXPECT_EQ(4, a);
    EXPECT_EQ(3, b);
}

TEST(UtilsCommon, SubscriptionThreadSafe)
{
    nx::utils::Subscription<int> subscription;

    std::atomic<int> a(1);
    nx::utils::SubscriptionId subscription1 = nx::utils::kInvalidSubscriptionId;
    subscription.subscribe(
        [&](int v)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            a = v;
        },
        &subscription1);

    // notify from different thread
    nx::utils::thread thread([&]() { subscription.notify(3); });

    // wait for thread to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(a, 1);

    // event should be waited here
    subscription.removeSubscription(subscription1);
    EXPECT_EQ(a, 3);

    thread.join();
}
