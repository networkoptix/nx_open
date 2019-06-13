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
        key.recursive = m_mode == nx::utils::Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;
        key.lockRecursionDepth = ++m_lockRecursionDepth;

        m_analyzer->afterMutexLocked(key);
    }

    void unlock()
    {
        MutexLockKey key;
        key.recursive = m_mode == nx::utils::Mutex::Recursive;
        key.threadHoldingMutex = m_threadHoldingMutex;
        key.lockRecursionDepth = --m_lockRecursionDepth;

        m_analyzer->beforeMutexUnlocked(key);

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
            [this]() { m_detectedDeadlocks.push_back(0); });
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

private:
    std::deque<int /*dummy*/> m_detectedDeadlocks;
};

TEST_F(MutexLockAnalyzer, mutex_deadlock_is_reported)
{
    DummyMutex m1(&m_analyzer);
    m1.lock();
    m1.lock();

    assertDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, recursive_mutex_does_not_produce_deadlock)
{
    DummyMutex m1(&m_analyzer, nx::utils::Mutex::Recursive);
    m1.lock();
    m1.lock();

    assertNoDeadlockIsReported();
}

TEST_F(MutexLockAnalyzer, DISABLED_recursive_mutex_does_not_produce_deadlock_2)
{
    DummyMutex m1(&m_analyzer, nx::utils::Mutex::Recursive);
    DummyMutex m2(&m_analyzer);

    auto t = std::thread(
        [&m1, &m2]()
        {
            m1.lock();
            m2.lock();
            m2.unlock();
            m1.unlock();
        });
    t.join();

    m1.lock();
    m2.lock();
    m1.lock();
    m1.unlock();
    m2.unlock();
    m1.unlock();

    assertNoDeadlockIsReported();
}

} // namespace nx::utils::test
