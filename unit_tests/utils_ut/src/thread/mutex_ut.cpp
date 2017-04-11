#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/log/log.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {
namespace test {

TEST(MutexAnalyzer, DISABLED_Deadlock2)
{
    QnMutex m1;
    QnMutex m2;

    utils::thread t1(
        [&]()
        {
            QnMutexLocker lock1(&m1);
            QnMutexLocker lock2(&m2);
            NX_LOG(lm("Thread 1"), cl_logDEBUG1);
        });


    utils::thread t2(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock1(&m1);
            NX_LOG(lm("Thread 2"), cl_logDEBUG1);
        });

    t1.join();
    t2.join();
}

TEST(MutexAnalyzer, DISABLED_Deadlock3)
{
    QnMutex m1;
    QnMutex m2;
    QnMutex m3;

    utils::thread t1(
        [&]()
        {
            QnMutexLocker lock1(&m1);
            QnMutexLocker lock2(&m2);
            NX_LOG(lm("Thread 1"), cl_logDEBUG1);
        });


    utils::thread t2(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock3(&m3);
            NX_LOG(lm("Thread 2"), cl_logDEBUG1);
        });

    utils::thread t3(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock3(&m3);
            QnMutexLocker lock1(&m1);
            NX_LOG(lm("Thread 3"), cl_logDEBUG1);
        });

    t1.join();
    t2.join();
    t3.join();
}

} // namespace test
} // namespace utils
} // namespace nx
