// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/thread/run_or_wait.h>

namespace nx::utils::test {

TEST(RunOrWait, Fixture)
{
    {
        std::atomic<int> calls = 0;
        RunOrWait<int, int> runOrWait(
            [&](int)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                ++calls;
                return 1;
            });
        {
            std::array<std::future<int>, 5> futures;
            for (auto& f: futures)
                f = std::async(std::launch::async, [&]() { return runOrWait(1); });
        }
        ASSERT_EQ(calls, 1);
    }

    std::promise<void> started;
    std::future<int> future;
    {
        RunOrWait<int, int> runOrWait(
            [&](int)
            {
                started.set_value();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return 1;
            });
        future = std::async(std::launch::async, [&]() { return runOrWait(1); });
        started.get_future().wait();
    }
}

} // namespace nx::utils::test
