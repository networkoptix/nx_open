// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <deque>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>
#include <nx/utils/thread/thread_util.h>

namespace nx::test {

class DummyMutex
{
public:
    DummyMutex(
        MutexLockAnalyzer* analyzer,
        Mutex::RecursionMode mode = Mutex::NonRecursive)
        :
        m_analyzer(analyzer),
        m_mode(mode)
    {
    }

    void lock()
    {
        m_threadHoldingMutex = ::currentThreadSystemId();

        MutexLockKey key;
        key.mutexPtr = reinterpret_cast<decltype(key.mutexPtr)>(this);
        key.recursive = m_mode == Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;

        ++m_lockRecursionDepth;

        m_analyzer->afterMutexLocked(key);
    }

    void unlock()
    {
        MutexLockKey key;
        key.mutexPtr = reinterpret_cast<decltype(key.mutexPtr)>(this);
        key.recursive = m_mode == Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;

        m_analyzer->beforeMutexUnlocked(key);

        --m_lockRecursionDepth;
        if (m_lockRecursionDepth == 0)
            m_threadHoldingMutex = 0;
    }

private:
    MutexLockAnalyzer* m_analyzer = nullptr;
    Mutex::RecursionMode m_mode = Mutex::NonRecursive;
    int m_lockRecursionDepth = 0;
    uintptr_t m_threadHoldingMutex = 0;
};

//-------------------------------------------------------------------------------------------------

class MutexLockAnalyzer:
    public ::testing::Test
{
public:
    MutexLockAnalyzer()
    {
        m_analyzer.setDeadlockDetectedHandler(
            [this](const auto& /*message*/)
            {
                m_detectedDeadlocks.push_back(0);
            });
    }

protected:
    nx::MutexLockAnalyzer m_analyzer;

    void assertDeadlockIsReported()
    {
        ASSERT_TRUE(!m_detectedDeadlocks.empty());
    }

    void assertNoDeadlockIsReported()
    {
        ASSERT_TRUE(m_detectedDeadlocks.empty());
    }

    void lockInANewThread(const std::vector<DummyMutex*>& mutexes)
    {
        std::thread(
            [&mutexes]()
            {
                std::for_each(mutexes.begin(), mutexes.end(), std::mem_fn(&DummyMutex::lock));
                std::for_each(mutexes.rbegin(), mutexes.rend(), std::mem_fn(&DummyMutex::unlock));
            }
        ).join();
    }

private:
    std::deque<int /*dummy*/> m_detectedDeadlocks;
};

TEST_F(MutexLockAnalyzer, recursive_lock_of_non_recursive_mutex_is_deadlock)
{
    DummyMutex m1(&m_analyzer);
    m1.lock();
    m1.lock();

    assertDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, recursive_lock_of_recursive_mutex_is_not_a_deadlock)
{
    DummyMutex m1(&m_analyzer, nx::Mutex::Recursive);
    m1.lock();
    m1.lock();

    assertNoDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, recursive_lock_of_recursive_mutex_is_not_a_deadlock_2)
{
    DummyMutex m1(&m_analyzer, nx::Mutex::Recursive);
    DummyMutex m2(&m_analyzer);

    lockInANewThread({&m1, &m2});
    lockInANewThread({&m1, &m2, &m1});

    assertNoDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, deadlock_between_two_threads_is_detected)
{
    DummyMutex m1(&m_analyzer);
    DummyMutex m2(&m_analyzer);

    lockInANewThread({&m1, &m2});
    lockInANewThread({&m2, &m1});

    assertDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, deadlock_between_N_threads_is_detected)
{
    static constexpr int kMutexCount = 5;

    std::vector<DummyMutex> mutexes(kMutexCount, DummyMutex(&m_analyzer));
    for (int i = 0; i < kMutexCount; ++i)
        lockInANewThread({&mutexes[i], &mutexes[(i+1) % kMutexCount]});

    assertDeadlockIsReported();
}

} // namespace nx::test
