#include <future>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace utils {
namespace test {

TEST(TimerManager, singleShot)
{
    const std::chrono::seconds delay(1);

    nx::utils::promise<void> triggeredPromise;

    TimerManager timerManager;
    timerManager.start();

    timerManager.addTimer(
        [&triggeredPromise](TimerId) { triggeredPromise.set_value(); },
        delay);

    ASSERT_EQ(
        std::future_status::ready,
        triggeredPromise.get_future().wait_for(delay*2));
}

TEST(TimerManager, nonStopTimer)
{
    const std::chrono::milliseconds delay(250);

    TimerManager timerManager;
    timerManager.start();

    std::atomic<int> eventCount(0);
    const auto timerId = timerManager.addNonStopTimer(
        [&eventCount](TimerId) { ++eventCount; },
        delay,
        delay);

    std::this_thread::sleep_for(delay * 7);
    timerManager.joinAndDeleteTimer(timerId);

    const auto eventCountSnapshot = eventCount.load();
    ASSERT_GT(eventCountSnapshot, 4);

    std::this_thread::sleep_for(delay * 4);
    ASSERT_EQ(eventCountSnapshot, eventCount.load());

    timerManager.stop();
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

} // namespace test
} // namespace utils
} // namespace nx
