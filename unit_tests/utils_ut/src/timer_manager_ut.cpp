/**********************************************************
* apr 12, 2016
* a.kolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>

#include <nx/utils/timer_manager.h>


namespace nx {
namespace utils {
namespace test {

TEST(TimerManager, singleShot)
{
    const std::chrono::seconds delay(1);

    TimerManager timerManager;
    timerManager.start();

    std::promise<void> triggeredPromise;
    timerManager.addTimer(
        [&triggeredPromise](TimerId) { triggeredPromise.set_value(); },
        delay);

    ASSERT_EQ(
        std::future_status::ready,
        triggeredPromise.get_future().wait_for(delay*2));

    timerManager.stop();
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

}   //test
}   //utils
}   //nx
