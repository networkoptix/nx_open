// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/timer_holder.h>

namespace nx::utils::test {

using namespace std::chrono_literals;

namespace {

bool waitUntil(std::chrono::milliseconds timeout, const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (condition())
            return true;

        std::this_thread::sleep_for(2ms);
    }

    return condition();
}

} // namespace

class TimerHolderTest: public ::testing::Test
{
public:
    TimerHolderTest():
        m_timerHolder([this]() { return &m_timerManager; })
    {
    }

protected:
    nx::utils::TimerManager m_timerManager;
    nx::utils::TimerHolder m_timerHolder;
};

TEST_F(TimerHolderTest, singleShotTimerFiresOnce)
{
    std::atomic<int> callCount = 0;

    m_timerHolder.addTimer("owner", [&callCount]() { ++callCount; }, 30ms);

    ASSERT_TRUE(waitUntil(2s, [&callCount]() { return callCount.load() == 1; }));

    std::this_thread::sleep_for(100ms);
    ASSERT_EQ(1, callCount.load());
}

TEST_F(TimerHolderTest, addTimerReplacesPreviousTimerForSameOwner)
{
    std::atomic<int> firstCallbackCalls = 0;
    std::atomic<int> secondCallbackCalls = 0;

    m_timerHolder.addTimer(
        "owner",
        [&firstCallbackCalls]() { ++firstCallbackCalls; },
        250ms);

    m_timerHolder.addTimer(
        "owner",
        [&secondCallbackCalls]() { ++secondCallbackCalls; },
        30ms);

    ASSERT_TRUE(waitUntil(2s, [&secondCallbackCalls]() { return secondCallbackCalls.load() == 1; }));

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, firstCallbackCalls.load());
    ASSERT_EQ(1, secondCallbackCalls.load());
}

TEST_F(TimerHolderTest, cancelTimerSyncCancelsTimer)
{
    std::atomic<int> callCount = 0;

    m_timerHolder.addTimer("owner", [&callCount]() { ++callCount; }, 200ms);
    m_timerHolder.cancelTimerSync("owner");

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, callCount.load());
}

TEST_F(TimerHolderTest, cancelAllTimersSyncCancelsTimersForAllOwners)
{
    std::atomic<int> firstCallCount = 0;
    std::atomic<int> secondCallCount = 0;

    m_timerHolder.addTimer("owner1", [&firstCallCount]() { ++firstCallCount; }, 200ms);
    m_timerHolder.addTimer("owner2", [&secondCallCount]() { ++secondCallCount; }, 200ms);

    m_timerHolder.cancelAllTimersSync();

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, firstCallCount.load());
    ASSERT_EQ(0, secondCallCount.load());
}

TEST_F(TimerHolderTest, terminateCancelsCurrentTimersAndPreventsNewTimers)
{
    std::atomic<int> oldTimerCallCount = 0;
    std::atomic<int> newTimerCallCount = 0;

    m_timerHolder.addTimer("owner1", [&oldTimerCallCount]() { ++oldTimerCallCount; }, 200ms);

    m_timerHolder.terminate();
    m_timerHolder.addTimer("owner2", [&newTimerCallCount]() { ++newTimerCallCount; }, 30ms);

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, oldTimerCallCount.load());
    ASSERT_EQ(0, newTimerCallCount.load());
}

TEST_F(TimerHolderTest, cancelUnknownOwnerDoesNotAffectExistingTimer)
{
    std::atomic<int> callCount = 0;

    m_timerHolder.addTimer("owner", [&callCount]() { ++callCount; }, 30ms);
    m_timerHolder.cancelTimerSync("missing");

    ASSERT_TRUE(waitUntil(2s, [&callCount]() { return callCount.load() == 1; }));

    std::this_thread::sleep_for(100ms);
    ASSERT_EQ(1, callCount.load());
}

TEST_F(TimerHolderTest, cancelledTimerCanBeScheduledAgainForSameOwner)
{
    std::atomic<int> firstCallbackCalls = 0;
    std::atomic<int> secondCallbackCalls = 0;

    m_timerHolder.addTimer(
        "owner",
        [&firstCallbackCalls]() { ++firstCallbackCalls; },
        250ms);
    m_timerHolder.cancelTimerSync("owner");

    m_timerHolder.addTimer(
        "owner",
        [&secondCallbackCalls]() { ++secondCallbackCalls; },
        30ms);

    ASSERT_TRUE(waitUntil(2s, [&secondCallbackCalls]() { return secondCallbackCalls.load() == 1; }));

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, firstCallbackCalls.load());
    ASSERT_EQ(1, secondCallbackCalls.load());
}

TEST_F(TimerHolderTest, terminateIsIdempotent)
{
    std::atomic<int> callCount = 0;

    m_timerHolder.addTimer("owner", [&callCount]() { ++callCount; }, 200ms);
    m_timerHolder.terminate();
    m_timerHolder.terminate();

    std::this_thread::sleep_for(350ms);
    ASSERT_EQ(0, callCount.load());
}

} // namespace nx::utils::test
