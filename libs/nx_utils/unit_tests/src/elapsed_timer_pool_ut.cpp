// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace test {

class ElapsedTimerPool:
    public ::testing::Test
{
public:
    ElapsedTimerPool():
        m_timerPool(std::bind(&ElapsedTimerPool::saveInvokedTimer, this,
            std::placeholders::_1)),
        m_timeShift(utils::test::ClockType::steady),
        m_timeout(1)
    {
    }

protected:
    void addTimer()
    {
        m_timerPool.addTimer(++m_prevTimerId, m_timeout);
    }

    void removeTimer()
    {
        m_timerPool.removeTimer(m_prevTimerId);
    }

    void replaceTimer()
    {
        m_timerPool.addTimer(m_prevTimerId, m_timeout);
    }

    void waitForTimerDeadline()
    {
        m_timeShift.applyRelativeShift(m_timeout);
    }

    void assertTimerIsInvoked()
    {
        m_timerPool.processTimers();

        ASSERT_TRUE(std::count(
            m_invokedTimers.begin(), m_invokedTimers.end(),
            m_prevTimerId) > 0);
    }

    void assertTimerIsNotInvoked()
    {
        m_timerPool.processTimers();

        ASSERT_TRUE(std::count(
            m_invokedTimers.begin(), m_invokedTimers.end(),
            m_prevTimerId) == 0);
    }

private:
    utils::ElapsedTimerPool<int> m_timerPool;
    int m_prevTimerId = 0;
    std::vector<int> m_invokedTimers;
    nx::utils::test::ScopedTimeShift m_timeShift;
    const std::chrono::hours m_timeout;

    void saveInvokedTimer(int timerId)
    {
        m_invokedTimers.push_back(timerId);
    }
};

TEST_F(ElapsedTimerPool, timer_works)
{
    addTimer();
    waitForTimerDeadline();
    assertTimerIsInvoked();
}

TEST_F(ElapsedTimerPool, timer_can_be_removed)
{
    addTimer();
    removeTimer();

    waitForTimerDeadline();

    assertTimerIsNotInvoked();
}

TEST_F(ElapsedTimerPool, timer_can_be_replaced)
{
    addTimer();
    waitForTimerDeadline();

    replaceTimer();
    assertTimerIsNotInvoked();

    waitForTimerDeadline();
    assertTimerIsInvoked();
}

TEST(ElapsedTimerPoolEarliest, fires_single_earliest_elapsed_timer)
{
    ScopedTimeShift timeShift(ClockType::steady);
    std::vector<int> invoked;
    utils::ElapsedTimerPool<int> pool([&invoked](int id) { invoked.push_back(id); });

    pool.addTimer(1, std::chrono::seconds(1));
    pool.addTimer(2, std::chrono::seconds(2));

    // Nothing elapsed yet.
    ASSERT_FALSE(pool.processEarliestTimer());
    ASSERT_TRUE(invoked.empty());

    timeShift.applyRelativeShift(std::chrono::seconds(1));

    // Only the earliest (id 1) elapsed: fires exactly one and reports it.
    ASSERT_TRUE(pool.processEarliestTimer());
    ASSERT_EQ((std::vector<int>{1}), invoked);

    // Id 2 is not elapsed yet.
    ASSERT_FALSE(pool.processEarliestTimer());
    ASSERT_EQ((std::vector<int>{1}), invoked);

    timeShift.applyRelativeShift(std::chrono::seconds(1));
    ASSERT_TRUE(pool.processEarliestTimer());
    ASSERT_EQ((std::vector<int>{1, 2}), invoked);

    // No timers left.
    ASSERT_FALSE(pool.processEarliestTimer());
}

TEST(ElapsedTimerPoolEarliest, drains_one_per_call_in_deadline_order)
{
    ScopedTimeShift timeShift(ClockType::steady);
    std::vector<int> invoked;
    utils::ElapsedTimerPool<int> pool([&invoked](int id) { invoked.push_back(id); });

    pool.addTimer(10, std::chrono::seconds(2));
    pool.addTimer(20, std::chrono::seconds(1)); //< Earlier deadline.

    timeShift.applyRelativeShift(std::chrono::seconds(3)); //< Both elapsed.

    ASSERT_TRUE(pool.processEarliestTimer());
    ASSERT_TRUE(pool.processEarliestTimer());
    ASSERT_FALSE(pool.processEarliestTimer());

    // The earlier-deadline timer (20) is drained first.
    ASSERT_EQ((std::vector<int>{20, 10}), invoked);
}

} // namespace test
} // namespace utils
} // namespace nx
