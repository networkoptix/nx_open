#include <gtest/gtest.h>
#include <utils/common/subscription.h>

#include <atomic>
#include <thread>

TEST(UtilsCommon, SubscriptionGuard)
{
    Subscription<int> subscription;

    int a = 1;
    auto ga = subscription.subscribe([&](int v) { a = v; });

    subscription.notify(2);
    EXPECT_EQ(a, 2);

    int b = 1;
    {
        auto gb = subscription.subscribe([&](int v) { b = v; });

        subscription.notify(3);
        EXPECT_EQ(a, 3);
        EXPECT_EQ(b, 3);
    }

    subscription.notify(4);
    EXPECT_EQ(a, 4);
    EXPECT_EQ(b, 3);
}

TEST(UtilsCommon, SubscriptionThreadSafe)
{
    Subscription<int> subscription;

    std::atomic<int> a(1);
    auto ga = subscription.subscribe([&](int v)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        a = v;
    });

    // notify from different thread
    std::thread thread([&]() { subscription.notify(3); });

    // wait for thread to start
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    EXPECT_EQ(a, 1);

    // event should be waited here
    ga.fire();
    EXPECT_EQ(a, 3);

    thread.join();
}
