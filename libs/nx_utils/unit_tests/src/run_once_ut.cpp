// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/run_once.h>

#if defined(Q_OS_UNIX)
    #include <sys/resource.h>
#endif

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

#if defined(Q_OS_UNIX)

// Thread creation may fail on low-memory devices (e.g. 32-bit ARM). RunOnce must report the
// failure instead of letting std::system_error escape and terminate the process - and must roll
// its state back so that a later call is able to retry.
class RunOnceUnderThreadLimit: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        rlimit prevLimit{};
        ASSERT_EQ(0, getrlimit(RLIMIT_NPROC, &prevLimit));
        // The limit counts every thread of the real user id, so any value that low makes the next
        // pthread_create() fail with EAGAIN. Already running threads are not affected.
        const rlimit tightLimit{1, prevLimit.rlim_max};
        if (setrlimit(RLIMIT_NPROC, &tightLimit) != 0)
        {
            GTEST_SKIP() << "Unable to lower RLIMIT_NPROC";
            return;
        }
        m_savedLimit = std::move(prevLimit);
    }

    virtual void TearDown() override { restoreThreadLimit(); }

    void restoreThreadLimit()
    {
        if (auto limit = std::exchange(m_savedLimit, std::nullopt); limit.has_value())
        {
            ASSERT_EQ(0, setrlimit(RLIMIT_NPROC, &*limit));
        }
    }

    /**
     * Both RunOnce flavours must behave the same way, so the check is shared. The key pack is
     * empty for RunOnce<void> and holds the single key for the keyed one.
     */
    template<typename RunOnceType, typename... Key>
    void expectFailureIsReportedAndRecoverable(RunOnceType& runOnce, const Key&... key)
    {
        std::atomic<int> counter = 0;
        auto worker = [&counter]()
        {
            ++counter;
        };

        ASSERT_FALSE(runOnce.startOnceAsync(worker, key...));
        ASSERT_FALSE(runOnce.restartOnceAsync(worker, key...));
        ASSERT_EQ(0, counter);

        restoreThreadLimit();

        auto task = runOnce.startOnceAsync(worker, key...);
        ASSERT_TRUE(task); //< The state has been rolled back, so the retry starts the task.
        task->wait();
        ASSERT_EQ(1, counter);
    }

private:
    std::optional<rlimit> m_savedLimit;
};

TEST_F(RunOnceUnderThreadLimit, withoutKey)
{
    RunOnce<void> runOnce;
    expectFailureIsReportedAndRecoverable(runOnce);
}

TEST_F(RunOnceUnderThreadLimit, withKey)
{
    RunOnce<int> runOnce;
    expectFailureIsReportedAndRecoverable(runOnce, 1);
}

#endif // defined(Q_OS_UNIX)

} // namespace nx::utils::test
