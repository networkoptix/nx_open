// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>
#include <set>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/async_object_pool.h>
#include <nx/network/aio/basic_pollable.h>

namespace nx::network::aio::test {

class AsyncObjectPool: public ::testing::Test
{
protected:
    static constexpr int kThreadCount = 64;

    struct Worker: BasicPollable
    {
        using BasicPollable::BasicPollable;
    };

    ~AsyncObjectPool() { m_service.pleaseStopSync(); }

    AIOService m_service;
};

// The pool used to be keyed by thread ids that AIO threads publish only once running, so a pool
// created right after AIOService::initialize() returned nullptr in threads that started late.
TEST_F(AsyncObjectPool, provides_a_distinct_worker_in_every_aio_thread_right_after_startup)
{
    ASSERT_TRUE(m_service.initialize(kThreadCount));
    aio::AsyncObjectPool<Worker> pool(
        &m_service, [this]() { return std::make_unique<Worker>(&m_service, nullptr); });

    std::set<Worker*> workers;
    for (auto thread: m_service.getAllAioThreads())
    {
        BasicPollable pollable(&m_service, thread);
        std::promise<Worker*> worker;
        pollable.post([&]() { worker.set_value(pool.get()); });
        const auto result = worker.get_future().get();
        pollable.pleaseStopSync();

        ASSERT_NE(nullptr, result);
        ASSERT_TRUE(workers.insert(result).second);
    }
}

} // namespace nx::network::aio::test
