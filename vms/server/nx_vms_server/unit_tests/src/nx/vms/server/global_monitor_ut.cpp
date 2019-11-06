#include <gtest/gtest.h>

#include <thread>
#include <platform/platform_abstraction.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::test {

TEST(GlobalMonitor, threadCount)
{
    nx::utils::TimerManager timerManager;
    timerManager.stop(); //< Don't create one more thread async.
    QnPlatformAbstraction platform(/*rootFs*/ nullptr, &timerManager);
    static const int kThreadsToCreate = 10;

    std::vector<std::thread> threads;
    std::promise<void> promise;
    std::shared_future<void> future(promise.get_future());

    int threadsBefore = platform.monitor()->thisProcessThreads();
    for (int i = 0; i < kThreadsToCreate; ++i)
    {
        threads.emplace_back(
            std::thread([&future]() { future.wait(); }
        ));
    }
    int threadsAfter = platform.monitor()->thisProcessThreads();

    promise.set_value();
    for (auto& thread: threads)
        thread.join();

    int threadsAfterStop = platform.monitor()->thisProcessThreads();

    EXPECT_EQ(threadsAfter, threadsBefore + kThreadsToCreate);
    EXPECT_EQ(threadsBefore, threadsAfterStop);
}

} // namespace nx::vms::server::test
