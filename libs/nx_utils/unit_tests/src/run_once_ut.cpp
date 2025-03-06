// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/run_once.h>

namespace nx::utils::test {


TEST(RunOnce, withoutKey)
{
    RunOnce<void> runOnce;

    nx::Mutex mutex;
    std::atomic<int> counter = 0;

    auto worker =
        [&]()
        {
            NX_MUTEX_LOCKER lock(&mutex);
            ++counter;
        };

    auto task = *runOnce.startOnceAsync(worker);
    task.wait();
    ASSERT_EQ(1, counter);

    mutex.lock();
    for (int i = 0; i < 10; ++i)
    {
        if (auto newTask = runOnce.restartOnceAsync(worker))
        {
            ASSERT_EQ(0, i);
            task = std::move(*newTask);
        }
    }
    mutex.unlock();
    task.wait();
    ASSERT_EQ(3, counter);
}

TEST(RunOnce, withKey)
{
    std::vector<int> counters(5);

    RunOnce<int> runOnce;
    std::map<int, RunOnceBase::Future> tasks;
    nx::Mutex mutex;

    auto worker = [&counters, &mutex](int index)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        ++counters[index];
    };

    for (size_t i = 0; i < counters.size(); ++i)
    {
        if (auto task = runOnce.startOnceAsync(std::bind(worker, i), i))
            tasks[i] = std::move(*task);
    }
    tasks.clear(); //< wait for done

    for (size_t i = 0; i < counters.size(); ++i)
        ASSERT_EQ(1, counters[i]);

    mutex.lock();
    for (size_t i = 0; i < counters.size(); ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            if (auto task = runOnce.restartOnceAsync(std::bind(worker, i), i))
                tasks[i] = std::move(*task);
        }
    }
    mutex.unlock();
    tasks.clear(); //< wait for done
    for (size_t i = 0; i < counters.size(); ++i)
        ASSERT_EQ(3, counters[i]);
}

} // namespace nx::utils::test
