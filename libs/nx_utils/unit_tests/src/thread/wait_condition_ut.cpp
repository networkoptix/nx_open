// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>

#include <gtest/gtest.h>

#include <nx/utils/thread/wait_condition.h>

namespace nx::test {

class WaitCondition:
    public ::testing::Test
{
protected:
    void waitForConditionAsync(std::chrono::milliseconds timeout)
    {
        m_waitCompleted = std::async(
            [this, timeout]() mutable
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                bool result = m_condVar.waitFor(
                    lock.mutex(),
                    timeout,
                    [this]()
                    {
                        ++m_predicatCallCounter;
                        return m_flag;
                    });
                return result;
            });
    }

    void satisfyCondition()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_flag = true;
        m_condVar.wakeAll();
    }

    void spuriousWakeup()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_flag = false;
        m_condVar.wakeAll();
    }

    void assertWaitCompletedWithResult(bool expected)
    {
        ASSERT_EQ(expected, m_waitCompleted.get());
    }

    int predicatCallCounter() const { return m_predicatCallCounter; }

private:
    std::future<bool> m_waitCompleted;
    nx::Mutex m_mutex;
    nx::WaitCondition m_condVar;
    bool m_flag = false;
    std::atomic<int> m_predicatCallCounter = 0;
};

TEST_F(WaitCondition, waitFor_returns_on_condition_satisfied)
{
    waitForConditionAsync(std::chrono::hours(24));
    satisfyCondition();
    assertWaitCompletedWithResult(true);
}

TEST_F(WaitCondition, waitFor_returns_on_timeout)
{
    waitForConditionAsync(std::chrono::milliseconds(10));
    assertWaitCompletedWithResult(false);
}

TEST_F(WaitCondition, wait_with_infinite_timeout)
{
    waitForConditionAsync(std::chrono::milliseconds::max());

    while (predicatCallCounter() < 3)
        spuriousWakeup();

    satisfyCondition();
    assertWaitCompletedWithResult(true);
}


} // namespace nx::test
