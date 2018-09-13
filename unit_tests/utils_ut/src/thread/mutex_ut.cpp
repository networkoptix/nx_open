#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/log/log.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {
namespace test {

static const auto kIncrementCount = 100 * 1000;
static const auto kRecursiveLockCount = 100;

TEST(Mutex, DISABLED_Performance)
{
    QnMutex mutex;
    int data = 0;
    for (int i = 0; i < kIncrementCount * kRecursiveLockCount; ++i)
    {
        QnMutexLocker lock(&mutex);
        ++data;
    }

    ASSERT_EQ(data, kIncrementCount * kRecursiveLockCount);
}

TEST(Mutex, DISABLED_PerformanceRecursive)
{
    QnMutex mutex(QnMutex::Recursive);
    int data = 0;
    for (int i = 0; i < kIncrementCount; ++i)
    {
        for (int j = 0; j < kRecursiveLockCount; ++j)
            mutex.lock();

        ++data;

        for (int j = 0; j < kRecursiveLockCount; ++j)
            mutex.unlock();
    }

    ASSERT_EQ(data, kIncrementCount);
}

TEST(MutexAnalyzer, DISABLED_Deadlock2)
{
    QnMutex m1;
    QnMutex m2;

    utils::thread t1(
        [&]()
        {
            QnMutexLocker lock1(&m1);
            QnMutexLocker lock2(&m2);
            NX_DEBUG(this, lm("Thread 1"));
        });


    utils::thread t2(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock1(&m1);
            NX_DEBUG(this, lm("Thread 2"));
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
            NX_DEBUG(this, lm("Thread 1"));
        });


    utils::thread t2(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock3(&m3);
            NX_DEBUG(this, lm("Thread 2"));
        });

    utils::thread t3(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            QnMutexLocker lock3(&m3);
            QnMutexLocker lock1(&m1);
            NX_DEBUG(this, lm("Thread 3"));
        });

    t1.join();
    t2.join();
    t3.join();
}

} // namespace test
} // namespace utils
} // namespace nx
