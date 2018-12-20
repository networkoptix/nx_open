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

TEST(MutexAnalyzer, DISABLED_Deadlock_2Threads)
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock1(&m1);
            NX_DEBUG(this, lm("Thread 2"));
        });

    t1.join();
    t2.join();
}

TEST(MutexAnalyzer, DISABLED_Deadlock_3Threads)
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            QnMutexLocker lock2(&m2);
            QnMutexLocker lock3(&m3);
            NX_DEBUG(this, lm("Thread 2"));
        });

    utils::thread t3(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            QnMutexLocker lock3(&m3);
            QnMutexLocker lock1(&m1);
            NX_DEBUG(this, lm("Thread 3"));
        });

    t1.join();
    t2.join();
    t3.join();
}

TEST(MutexAnalyzer, DISABLED_Deadlock_WaitCondition)
{
    Mutex m1;
    Mutex m2;
    WaitCondition c;

    thread t1(
        [&]()
        {
            NX_MUTEX_LOCKER lock1(&m1);
            c.wait(&m1);
            NX_MUTEX_LOCKER lock2(&m2);
            NX_DEBUG(this, lm("Thread 1"));
        });

    thread t2(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            NX_MUTEX_LOCKER lock2(&m2);
            NX_MUTEX_LOCKER lock1(&m1);
            c.wakeAll();
            NX_DEBUG(this, lm("Thread 2"));
        });

    t1.join();
    t2.join();
}

TEST(MutexAnalyzer, DISABLED_Deadlock_WaitCondition_2Waiters)
{
    Mutex m1;
    Mutex m2;
    WaitCondition c;

    thread t1(
        [&]()
        {
            NX_MUTEX_LOCKER lock1(&m1);
            c.wait(&m1);
            NX_MUTEX_LOCKER lock2(&m2);
            NX_DEBUG(this, lm("Thread 1"));
        });

    thread t2(
        [&]()
        {
            NX_MUTEX_LOCKER lock1(&m1);
            c.wait(&m1);
            NX_MUTEX_LOCKER lock2(&m2);
            NX_DEBUG(this, lm("Thread 2"));
        });

    thread t3(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (int i = 0; i < 2; ++i)
            {
                NX_MUTEX_LOCKER lock2(&m2);
                NX_MUTEX_LOCKER lock1(&m1);
                c.wakeOne();
            }
            NX_DEBUG(this, lm("Thread 3"));
        });

    t1.join();
    t2.join();
}

} // namespace test
} // namespace utils
} // namespace nx
