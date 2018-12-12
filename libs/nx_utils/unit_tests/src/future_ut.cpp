#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/random.h>

TEST(promise, general)
{
    nx::utils::promise<bool> promise;
    nx::utils::thread thread([&promise]{ promise.set_value(true); });
    std::this_thread::sleep_for(std::chrono::microseconds(
        nx::utils::random::number(0, 99999)));

    ASSERT_TRUE(promise.get_future().get());
    thread.join();
}
