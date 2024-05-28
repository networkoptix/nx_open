// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/thread/worker.h>

#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

namespace nx::utils::test {

class Worker: public ::testing::Test
{
protected:
    enum class QueueSize
    {
        shouldGrow,
        shouldNotGrow,
    };

    void givenWorkerWithoutMaxTaskLimit()
    {
        m_worker.reset(new nx::utils::Worker(std::nullopt));
    }

    void givenWorkerWithMaxTaskLimit()
    {
        m_worker.reset(new nx::utils::Worker(kMaxCount));
    }

    void whenMultipleTasksPosted(QueueSize sizePolicy, bool shouldPostFromSelf = false)
    {
        for (int i = 0; i < 10; i++)
            m_worker->post(createCb(sizePolicy, shouldPostFromSelf));
    }

    void whenTaskPostFromItselfAndWaitsForResult()
    {
        m_taskCount = 2;
        m_worker->post(
            [this]()
            {
                m_worker->post([this]() { m_taskCount--; });
                m_taskCount--;
            });
    }

    void thenAllOfThemProcessed()
    {
        while (m_taskCount != 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void thenQueueSizeMayHaveVaried() { ASSERT_TRUE(m_maxSizeSurspassed); }
    void thenQueueSizeShouldntHaveBeenGreaterThanLimit() { ASSERT_FALSE(m_maxSizeSurspassed); }

private:
    static constexpr size_t kMaxCount = 1;
    static constexpr size_t kPostedMultiplier = 3;

    std::unique_ptr<utils::Worker> m_worker;
    std::atomic<int> m_taskCount = 0;
    std::atomic<bool> m_maxSizeSurspassed = false;

    MoveOnlyFunc<void()> createCb(QueueSize sizePolicy, bool shouldPostFromSelf)
    {
        m_taskCount++;
        return
            [this, shouldPostFromSelf, sizePolicy]()
            {
                switch (sizePolicy)
                {
                    case QueueSize::shouldGrow:
                        while (!m_maxSizeSurspassed && m_worker->size() <= kMaxCount + 1)
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        m_maxSizeSurspassed = true;
                        break;
                    case QueueSize::shouldNotGrow:
                        if (m_worker->size() > kMaxCount + 1)
                            m_maxSizeSurspassed = true;
                        std::this_thread::sleep_for(std::chrono::milliseconds(3));
                        break;
                }

                if (shouldPostFromSelf)
                {
                    for (int i = 0; i < 3; i++)
                        m_worker->post(createCb(sizePolicy, false));
                }
                m_taskCount--;
            };
    }
};

TEST_F(Worker, AllTasksProcessed_NoMaxTaskCount)
{
    givenWorkerWithoutMaxTaskLimit();
    whenMultipleTasksPosted(QueueSize::shouldGrow);
    thenAllOfThemProcessed();
    thenQueueSizeMayHaveVaried();
}

TEST_F(Worker, AllTasksProcessed_MaxTaskCount)
{
    givenWorkerWithMaxTaskLimit();
    whenMultipleTasksPosted(QueueSize::shouldNotGrow);
    thenAllOfThemProcessed();
    thenQueueSizeShouldntHaveBeenGreaterThanLimit();
}

TEST_F(Worker, ShouldBePossibleToPostTasksFromCbEvenLimitIsSurpassed)
{
    givenWorkerWithMaxTaskLimit();
    whenMultipleTasksPosted(QueueSize::shouldNotGrow, /*shouldPostFromSelf*/ true);
    thenAllOfThemProcessed();
    thenQueueSizeMayHaveVaried();
}

TEST_F(Worker, CbPostedFromCbExecutedImmediately)
{
    givenWorkerWithoutMaxTaskLimit();
    whenTaskPostFromItselfAndWaitsForResult();
    thenAllOfThemProcessed();
}

} // namespace nx::utils::test
