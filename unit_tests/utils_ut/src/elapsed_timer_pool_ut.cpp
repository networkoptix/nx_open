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

} // namespace test
} // namespace utils
} // namespace nx
