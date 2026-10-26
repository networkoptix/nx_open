// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/thread/jthread.h>

namespace nx::utils::test {

class JThreadTestBase: public ::testing::Test
{
protected:
    void SetUp() override
    {
        if constexpr (kUseStdJThread)
            GTEST_SKIP() << "nx::utils::jthread is an alias of std::jthread; "
                            "the stl implementation is not tested here";
    }
};

using StopSource = JThreadTestBase;
using StopToken = JThreadTestBase;
using JThread = JThreadTestBase;

TEST_F(StopSource, requestStop)
{
    stop_source source;
    const auto token = source.get_token();
    ASSERT_TRUE(source.stop_possible());
    ASSERT_TRUE(token.stop_possible());
    ASSERT_FALSE(token.stop_requested());

    ASSERT_TRUE(source.request_stop());
    ASSERT_FALSE(source.request_stop());
    ASSERT_TRUE(source.stop_requested());
    ASSERT_TRUE(token.stop_requested());
}

TEST_F(StopSource, noStopState)
{
    stop_source source(nostopstate);
    ASSERT_FALSE(source.stop_possible());
    ASSERT_FALSE(source.request_stop());
    ASSERT_FALSE(source.get_token().stop_possible());
}

TEST_F(StopToken, defaultConstructed)
{
    const stop_token token;
    ASSERT_FALSE(token.stop_possible());
    ASSERT_FALSE(token.stop_requested());
}

TEST_F(StopToken, stopPossibleAfterSourcesDestroyed)
{
    stop_token notRequested;
    stop_token requested;
    {
        stop_source first;
        stop_source second;
        notRequested = first.get_token();
        requested = second.get_token();
        second.request_stop();
    }
    ASSERT_FALSE(notRequested.stop_possible());
    ASSERT_TRUE(requested.stop_possible());
    ASSERT_TRUE(requested.stop_requested());
}

TEST_F(JThread, destructorRequestsStopAndJoins)
{
    std::atomic<bool> stopped = false;
    {
        jthread thread(
            [&stopped](stop_token token)
            {
                while (!token.stop_requested())
                    std::this_thread::yield();
                stopped = true;
            });
        ASSERT_TRUE(thread.joinable());
    }
    ASSERT_TRUE(stopped);
}

TEST_F(JThread, callableWithoutStopToken)
{
    int result = 0;
    {
        jthread thread([&result](int a, int b) { result = a + b; }, 2, 3);
    }
    ASSERT_EQ(5, result);
}

TEST_F(JThread, requestStop)
{
    std::atomic<bool> stopped = false;
    jthread thread(
        [&stopped](stop_token token)
        {
            while (!token.stop_requested())
                std::this_thread::yield();
            stopped = true;
        });

    ASSERT_TRUE(thread.request_stop());
    thread.join();
    ASSERT_TRUE(stopped);
    ASSERT_TRUE(thread.get_stop_token().stop_requested());
}

TEST_F(JThread, moveAssignmentStopsPreviousThread)
{
    std::atomic<bool> firstStopped = false;
    jthread thread(
        [&firstStopped](stop_token token)
        {
            while (!token.stop_requested())
                std::this_thread::yield();
            firstStopped = true;
        });

    thread = jthread([] {});
    ASSERT_TRUE(firstStopped);
    ASSERT_TRUE(thread.joinable());
}

TEST_F(JThread, defaultConstructed)
{
    jthread thread;
    ASSERT_FALSE(thread.joinable());
    ASSERT_FALSE(thread.get_stop_token().stop_possible());
}

} // namespace nx::utils::test
