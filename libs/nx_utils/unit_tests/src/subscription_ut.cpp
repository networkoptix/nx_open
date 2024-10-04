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
protected:
    ~Subscription()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    void installRecursiveEventHandler(int recursionDepth, std::function<void()> func = nullptr)
    {
        m_subscription.subscribe(
            [this, func = std::move(func)](int v)
            {
                if (m_startedPromise && v == m_notifyParam)
                    m_startedPromise->set_value();

                if (v > 0)
                {
                    m_subscription.notify(v-1);
                    if (func)
                        func();
                }
                else if (m_donePromise)
                {
                    m_donePromise->set_value();
                }
            },
            &m_id);

        m_notifyParam = recursionDepth;
    }

    void installEventHandler(std::function<void()> func)
    {
        m_subscription.subscribe(
            [func = std::move(func)](auto&&...) { func(); },
            &m_id);
    }

    void whenNotify()
    {
        m_subscription.notify(1);
    }

    // Returns tuple<startedFuture, doneFuture>
    std::tuple<std::future<void>, std::future<void>> whenNotifyConcurrently()
    {
        m_startedPromise = std::make_unique<std::promise<void>>();
        m_donePromise = std::make_unique<std::promise<void>>();

        auto startedFuture = m_startedPromise->get_future();
        auto doneFuture = m_donePromise->get_future();

        m_thread = std::thread([this]() {
            m_subscription.notify(m_notifyParam);
        });

        return std::make_tuple(std::move(startedFuture), std::move(doneFuture));
    }

    void whenRemoveSubscription()
    {
        m_subscription.removeSubscription(m_id);
    }

private:
    nx::utils::Subscription<int> m_subscription;
    nx::utils::SubscriptionId m_id = 0;
    int m_notifyParam = -1;
    std::unique_ptr<std::promise<void>> m_startedPromise;
    std::unique_ptr<std::promise<void>> m_donePromise;
    std::thread m_thread;
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

    nx::utils::SubscriptionId id1 = 0;
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

TEST_F(Subscription, concurrent_removeSubscription_waits_for_all_recursive_notifications)
{
    installRecursiveEventHandler(
        /*recursionDepth*/ 17,
        []() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });

    auto [startedFuture, doneFuture] = whenNotifyConcurrently();
    startedFuture.wait();
    whenRemoveSubscription();

    // Making sure all notifications were completed by now.
    ASSERT_EQ(std::future_status::ready, doneFuture.wait_for(std::chrono::seconds::zero()));
}

TEST_F(Subscription, notify_is_exception_safe)
{
    installEventHandler([]() { throw std::runtime_error("test"); });

    try
    {
        whenNotify();
        GTEST_FAIL();
    }
    catch (const std::exception& e)
    {
        GTEST_ASSERT_EQ(std::string("test"), e.what());
    }

    whenRemoveSubscription();
}

} // namespace nx::utils::test
