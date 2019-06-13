#include <atomic>
#include <deque>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/thread/mutex_lock_analyzer.h>
#include <nx/utils/thread/thread_util.h>

namespace nx::utils::test {

class DummyMutex
{
public:
    DummyMutex(
        utils::MutexLockAnalyzer* analyzer,
        nx::utils::Mutex::RecursionMode mode = nx::utils::Mutex::NonRecursive)
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
        key.recursive = m_mode == nx::utils::Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;

        ++m_lockRecursionDepth;

        m_analyzer->afterMutexLocked(key);
    }

    void unlock()
    {
        MutexLockKey key;
        key.mutexPtr = reinterpret_cast<decltype(key.mutexPtr)>(this);
        key.recursive = m_mode == nx::utils::Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;

        m_analyzer->beforeMutexUnlocked(key);

        --m_lockRecursionDepth;
        if (m_lockRecursionDepth == 0)
            m_threadHoldingMutex = 0;
    }

private:
    utils::MutexLockAnalyzer* m_analyzer = nullptr;
    nx::utils::Mutex::RecursionMode m_mode = nx::utils::Mutex::NonRecursive;
    std::atomic<int> m_lockRecursionDepth{0};
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
    utils::MutexLockAnalyzer m_analyzer;

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
    DummyMutex m1(&m_analyzer, nx::utils::Mutex::Recursive);
    m1.lock();
    m1.lock();

    assertNoDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, deadlock_between_threads_is_detected)
{
    DummyMutex m1(&m_analyzer);
    DummyMutex m2(&m_analyzer);

    lockInANewThread({&m1, &m2});
    lockInANewThread({&m2, &m1});

    assertDeadlockIsReported();
}

} // namespace nx::utils::test
