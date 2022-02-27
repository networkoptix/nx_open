// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include <nx/utils/std/thread.h>
#include <nx/utils/subscription.h>

namespace nx::utils::test {

class Subscription:
    public ::testing::Test
{
};

TEST_F(Subscription, works)
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

TEST_F(Subscription, is_thread_safe)
{
    nx::utils::Subscription<int> subscription;

    std::atomic<int> a(1);
    nx::utils::SubscriptionId subscription1 = nx::utils::kInvalidSubscriptionId;
    std::promise<void> inHandler;
    subscription.subscribe(
        [&a, &inHandler](int v)
        {
            inHandler.set_value();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            a = v;
        },
        &subscription1);

    // notify from different thread
    nx::utils::thread thread([&]() { subscription.notify(3); });

    // wait for thread to start
    inHandler.get_future().wait();

    // event should be waited here
    subscription.removeSubscription(subscription1);
    EXPECT_EQ(a, 3);

    thread.join();
}

TEST_F(Subscription, recursive_notifications_are_supported)
{
    nx::utils::Subscription<int> subscription;

    std::vector<int> notifications;

    nx::utils::SubscriptionId id1 = -1;
    subscription.subscribe(
        [&subscription, &notifications](int v)
        {
            notifications.push_back(v);
            if (v < 2)
                subscription.notify(2);
        },
        &id1);

    subscription.notify(1);

    std::vector<int> expected{1, 2};
    ASSERT_EQ(expected, notifications);
}

} // namespace nx::utils::test
