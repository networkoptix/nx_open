#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/thread/cf/cfuture.h>

template<typename T>
cf::future<T> returnAsync(T value)
{
    cf::promise<T> promise;
    auto future = promise.get_future();
    std::thread thread(
        [promise = std::move(promise), value = std::move(value)]() mutable
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            promise.set_value(value);
        });

    thread.detach();
    return future;
}

TEST(CFuture, Sync)
{
    auto value1 = returnAsync(1);
    auto value2 = returnAsync(2);

    EXPECT_EQ(value1.get(), 1);
    EXPECT_EQ(value2.get(), 2);
}

TEST(CFuture, Async)
{
    std::atomic<int> value1(0);
    std::atomic<int> value2(0);

    returnAsync(1).then([&value1](auto i) { value1 = i.get(); return cf::unit(); });
    returnAsync(2).then([&value2](auto i) { value2 = i.get(); return cf::unit(); });

    while (!value1 || !value2)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(value1.load(), 1);
    EXPECT_EQ(value2.load(), 2);
}
