// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/aio_thread_watcher.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::aio::test {

class AioThreadWatcher: public ::testing::Test
{
public:
    AioThreadWatcher()
    {
        m_aioThreadWatcher = std::make_unique<aio::AioThreadWatcher>(
            /*pollRatePerSecond*/ 1000,
            /*averageTimeMultiplier*/ 100,
            /*absoluteThreshold*/ std::chrono::milliseconds(1),
            [this](auto stuckThread)
            {
                NX_INFO(this, "Got stuck thread: %1", stuckThread);
                m_newlyStuckThreads.push(stuckThread);
            });

        std::vector<const AbstractAioThread*> threadsToWatch;
        m_aioThreads.emplace_back(nullptr, m_aioThreadWatcher.get()).start();
        threadsToWatch.push_back(&m_aioThreads.back());

        m_aioThreadWatcher->startWatcherThread(threadsToWatch);

        m_pollable = std::make_unique<BasicPollable>(&m_aioThreads.back());
    }

    ~AioThreadWatcher()
    {
        m_unstick.push();
        while (!m_unstick.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    aio::AioThreadWatcher::StuckThread waitForStuckThread()
    {
       m_pollable->post([this](){ m_unstick.pop(); });
       return m_newlyStuckThreads.pop();
    }

protected:
    nx::utils::SyncQueue<aio::AioThreadWatcher::StuckThread> m_newlyStuckThreads;
    std::unique_ptr<aio::AioThreadWatcher> m_aioThreadWatcher;
    std::list<AioThread> m_aioThreads;
    nx::utils::SyncQueue<void> m_unstick;
    std::unique_ptr<BasicPollable> m_pollable;
};

TEST_F(AioThreadWatcher, reports_newly_stuck_thread)
{
    const auto stuckThread = waitForStuckThread();

    EXPECT_EQ(stuckThread.aioThread, &m_aioThreads.back());
    EXPECT_LE(m_aioThreadWatcher->absoluteThreshold(), stuckThread.currentExecutionTime);
    EXPECT_EQ("post", stuckThread.functionType);
    EXPECT_EQ(0, stuckThread.averageExecutionTime.count());
}

TEST_F(AioThreadWatcher, detectStuckThreads_always_returns_stuck_threads)
{
    waitForStuckThread();

    EXPECT_TRUE(m_newlyStuckThreads.empty());
    EXPECT_EQ(1, m_aioThreadWatcher->detectStuckThreads().size());
}

//-------------------------------------------------------------------------------------------------
// AioServiceStatistics

class AioServiceStatistics: public ::testing::Test
{

public:
    AioServiceStatistics()
    {
        NX_ASSERT(m_aioService.initialize(
            m_aioThreadPoolSize,
            true /*enableAioThreadWatcher*/));

        m_aioService.aioThreadWatcher()->setAbsoluteThreshold(std::chrono::milliseconds(10));

        for (auto& thread: m_aioService.getAllAioThreads())
            m_pollables.emplace_back(thread);
    }

    ~AioServiceStatistics()
    {
        for (int i = 0; i < (int) m_pollables.size(); ++i)
        {
            m_unstick.push();
            while (!m_unstick.empty())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    AioStatistics getStatisticsWithAllThreadsStuck()
    {
        for (auto& pollable: m_pollables)
            pollable.post([this](){ m_unstick.pop(); });

        auto statistics = m_aioService.statistics();
        while (statistics.stuckThreadData->count != (int) m_pollables.size())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            statistics = m_aioService.statistics();
        }

        return statistics;
    }

protected:
    int m_aioThreadPoolSize = 3;
    AIOService m_aioService;
    std::list<BasicPollable> m_pollables;
    nx::utils::SyncQueue<void> m_unstick;
};

TEST_F(AioServiceStatistics, statistics)
{
    auto statistics = getStatisticsWithAllThreadsStuck();

    ASSERT_EQ(m_aioThreadPoolSize, statistics.threadCount);
    ASSERT_EQ(m_aioThreadPoolSize, (int) statistics.threads.size());

    ASSERT_NE(std::nullopt, statistics.stuckThreadData);
    ASSERT_EQ(m_aioThreadPoolSize, statistics.stuckThreadData->count);

    ASSERT_EQ(0, statistics.queueSizesTotal);
    for (auto& [id, thread]: statistics.threads)
    {
        ASSERT_TRUE(nx::utils::contains(statistics.stuckThreadData->threadIds, id));
        ASSERT_EQ(0, thread.queueSize);
        ASSERT_TRUE(thread.stuck);
    }
}

} // namespace nx::network::aio::test
