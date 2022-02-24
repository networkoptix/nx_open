// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/log/log.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/semaphore.h>

namespace nx::test {

TEST(Semaphore, SingleThread)
{
    Semaphore semaphore(2);
    ASSERT_EQ(semaphore.available(), 2);

    semaphore.acquire(0); //< Does nothing.
    ASSERT_EQ(semaphore.available(), 2);

    semaphore.acquire(); //< Aquires 1.
    ASSERT_EQ(semaphore.available(), 1);

    semaphore.acquire(1); //< Aquires 1.
    ASSERT_EQ(semaphore.available(), 0);

    ASSERT_TRUE(semaphore.tryAcquire(0)); //< Does nothing.
    ASSERT_EQ(semaphore.available(), 0);
    ASSERT_FALSE(semaphore.tryAcquire()); //< Try to acquire 1 - no resources left.
    ASSERT_EQ(semaphore.available(), 0);
    ASSERT_FALSE(semaphore.tryAcquire(2)); //< Try to acquire 2 - no resources left.
    ASSERT_EQ(semaphore.available(), 0);

    semaphore.release(); //< Releases 1.
    ASSERT_EQ(semaphore.available(), 1);

    ASSERT_FALSE(semaphore.tryAcquire(2)); //< Try to acquire 2 - fails, only 1 is available.
    ASSERT_EQ(semaphore.available(), 1);
    ASSERT_TRUE(semaphore.tryAcquire(1)); //< Try to acquire 1 - fails, only 1 is available.
    ASSERT_EQ(semaphore.available(), 0);

    semaphore.release();
    ASSERT_EQ(semaphore.available(), 1);

    semaphore.release(2); //< Current impl allows releasing more than initially available.
    ASSERT_EQ(semaphore.available(), 3);
}

TEST(Semaphore, TwoThreads)
{
    Semaphore semaphore(1);
    ASSERT_EQ(semaphore.available(), 1);

    ASSERT_TRUE(semaphore.tryAcquire());
    ASSERT_EQ(semaphore.available(), 0);

    utils::thread thread(
        [&]()
        {
            NX_DEBUG(this, nx::format("Spawned thread"));
            ASSERT_EQ(semaphore.available(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            semaphore.release();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ASSERT_EQ(semaphore.available(), 0); //< Must be already acquired by the main thread.
        });

    NX_DEBUG(this, nx::format("Main thread"));

    ASSERT_FALSE(semaphore.tryAcquire()); //< Busy - the spawned thread did not release it yet.
    semaphore.acquire(); //< Wait for the spawned thread to release it.
    ASSERT_EQ(semaphore.available(), 0);

    thread.join();
}

} // namespace nx::test
