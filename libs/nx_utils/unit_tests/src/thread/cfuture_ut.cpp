// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <exception>
#include <thread>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/cf/sync_executor.h>

namespace {

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

cf::future<cf::unit> initiate()
{
    return cf::initiate([]() -> cf::unit { throw std::runtime_error("error"); });
}

} // namespace

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

TEST(CFuture, CatchException)
{
    cf::sync_executor exec;

    // .then(...)
    {
        auto future = initiate().then([](auto&&) { return cf::unit{}; });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future = initiate().then([](auto&&) { return cf::make_ready_future(cf::unit{}); });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future = initiate().then(exec, [](auto&&) { return cf::unit{}; });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future =
            initiate().then(exec, [](auto&&) { return cf::make_ready_future(cf::unit{}); });
        EXPECT_NO_THROW(future.get());
    }

    // .catch_(...)
    {
        auto future = initiate().catch_([](const std::exception&) { return cf::unit{}; });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future = initiate().catch_(
            [](const std::exception&) { return cf::make_ready_future(cf::unit{}); });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future = initiate().catch_(exec, [](const std::exception&) { return cf::unit{}; });
        EXPECT_NO_THROW(future.get());
    }
    {
        auto future = initiate().catch_(
            exec, [](const std::exception&) { return cf::make_ready_future(cf::unit{}); });
        EXPECT_NO_THROW(future.get());
    }
}
