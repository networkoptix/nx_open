/**********************************************************
* May 18, 2016
* a.kolesnikov
***********************************************************/

#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>


TEST(promise, general)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::utils::promise<bool> promise;
        std::thread thread([&promise]{ promise.set_value(true); });
        std::this_thread::sleep_for(std::chrono::microseconds(rand()));
        ASSERT_TRUE(promise.get_future().get());

        thread.join();
    }
}
