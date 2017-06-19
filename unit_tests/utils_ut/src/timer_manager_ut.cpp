#include <future>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace utils {
namespace test {

class TimerManager:
    public ::testing::Test
{
public:
    static constexpr std::chrono::milliseconds delay = std::chrono::milliseconds(250);

    TimerManager():
        m_eventCount(0),
        m_timerId(-1)
    {
        m_timerManager.start();
    }

    ~TimerManager()
    {
        m_timerManager.stop();
    }

protected:
    void startSingleShotTimer()
    {
        m_timerStartClock = std::chrono::steady_clock::now();
        m_timerId = m_timerManager.addTimer(
            std::bind(&TimerManager::onTimerEvent, this, std::placeholders::_1),
            delay);
    }

    void startNonStopTimer()
    {
        m_timerStartClock = std::chrono::steady_clock::now();
        m_timerId = m_timerManager.addNonStopTimer(
            std::bind(&TimerManager::onTimerEvent, this, std::placeholders::_1),
            delay,
            delay);
    }

    void waitForTimerEvent()
    {
        waitForEventCountToExceed(0);
    }

    void waitForEventCountToExceed(int count)
    {
        while (m_eventCount <= count)
            std::this_thread::sleep_for(delay / 10);
    }

    void killTimer()
    {
        m_timerManager.joinAndDeleteTimer(m_timerId);
        m_timerId = -1;
    }

    void assertTimerErrorFactorIsNotLargerThan(float coeff)
    {
        const auto minTime = std::chrono::duration_cast<
            std::remove_const<decltype(delay)>::type>(delay * (1 - coeff));
        const auto maxTime = std::chrono::duration_cast<
            std::remove_const<decltype(delay)>::type>(delay * (1 + coeff));

        const auto totalTime = m_lastTimerEventClock - m_timerStartClock;
        const auto averageEventTime = totalTime / m_eventCount.load();

        ASSERT_GE(averageEventTime, minTime);
        ASSERT_LE(averageEventTime, maxTime);
    }

    void assertNoTimerEventAreReportedFurther()
    {
        auto currentEventCount = m_eventCount.load();
        std::this_thread::sleep_for(delay * 4);
        ASSERT_EQ(currentEventCount, m_eventCount.load());
    }

private:
    utils::TimerManager m_timerManager;
    std::atomic<int> m_eventCount;
    TimerId m_timerId;
    std::chrono::steady_clock::time_point m_timerStartClock;
    std::chrono::steady_clock::time_point m_lastTimerEventClock;

    void onTimerEvent(TimerId)
    {
        ++m_eventCount;
        m_lastTimerEventClock = std::chrono::steady_clock::now();
    }
};

constexpr std::chrono::milliseconds TimerManager::delay;

TEST_F(TimerManager, single_shot_timer)
{
    startSingleShotTimer();
    waitForTimerEvent();
    assertTimerErrorFactorIsNotLargerThan(1.0);
}

TEST_F(TimerManager, non_stop_timer)
{
    startNonStopTimer();
    waitForEventCountToExceed(4);
    killTimer();
    assertTimerErrorFactorIsNotLargerThan(1.0);
}

TEST_F(TimerManager, non_stop_timer_stops_after_cancellation)
{
    startNonStopTimer();
    waitForTimerEvent();
    killTimer();
    assertNoTimerEventAreReportedFurther();
}

TEST(parseTimerDuration, correct_input)
{
    using namespace std::chrono;

    const auto defaultValue = seconds(5);

    ASSERT_EQ(seconds(1), parseTimerDuration("1s", defaultValue));
    ASSERT_EQ(seconds(1), parseTimerDuration("1000ms", defaultValue));
    ASSERT_EQ(hours(1), parseTimerDuration("1h", defaultValue));
    ASSERT_EQ(hours(24), parseTimerDuration("1d", defaultValue));
    ASSERT_EQ(seconds(0), parseTimerDuration("0s", defaultValue));
}

TEST(parseTimerDuration, incorrect_input)
{
    using namespace std::chrono;

    const auto defaultValue = seconds(5);

    ASSERT_EQ(defaultValue, parseTimerDuration("s", defaultValue));
    ASSERT_EQ(defaultValue, parseTimerDuration("", defaultValue));
}

TEST(deleteFromCallback, simple)
{
    utils::StandaloneTimerManager mgr;
    int fireCount = 0;
    const std::chrono::milliseconds kTimeout(10);

    mgr.addNonStopTimer(
        [&mgr, &fireCount](nx::utils::TimerId timerId)
        {
            ++fireCount;
            mgr.deleteTimer(timerId);
        },
        kTimeout,
        kTimeout);

    std::this_thread::sleep_for(std::chrono::milliseconds(kTimeout * 10));
    mgr.stop();
    ASSERT_EQ(fireCount, 1);
}

} // namespace test
} // namespace utils
} // namespace nx
