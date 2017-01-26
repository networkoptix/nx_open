#include <gtest/gtest.h>

#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>

#include <core/resource/general_attribute_pool.h>

namespace test {

template<typename KeyType, typename MappedType>
class TestGeneralAttributePool:
    public ::QnGeneralAttributePool<KeyType, MappedType>
{
public:
    template<typename OnStartedWaitingForUnlock>
    void clear(OnStartedWaitingForUnlock onStartedWaitingForUnlock)
    {
        ::QnGeneralAttributePool<KeyType, MappedType>::clear(
            std::move(onStartedWaitingForUnlock));
    }
};

using Items = TestGeneralAttributePool<int, int>;

class LockItemThread
{
public:
    LockItemThread(Items* items, int id):
        m_items(items),
        m_id(id)
    {
        m_itemHasBeenLockedFuture = m_itemHasBeenLockedPromise.get_future();
        m_itemCanBeUnlockedFuture = m_itemCanBeUnlockedPromise.get_future();
    }

    ~LockItemThread()
    {
        join();
    }

    void join()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    void start()
    {
        m_thread = nx::utils::thread(std::bind(&LockItemThread::lockItemThreadFunc, this));
    }

    nx::utils::future<void>& itemHasBeenLockedFuture()
    {
        return m_itemHasBeenLockedFuture;
    }

    void unlockItemAsync()
    {
        m_itemCanBeUnlockedPromise.set_value();
    }

    int value() const
    {
        Items::ScopedLock lock(m_items, m_id);
        return *lock;
    }

    void setValue(int val)
    {
        Items::ScopedLock lock(m_items, m_id);
        *lock = val;
    }

private:
    nx::utils::thread m_thread;
    Items* m_items;
    const int m_id;
    nx::utils::promise<void> m_itemHasBeenLockedPromise;
    nx::utils::future<void> m_itemHasBeenLockedFuture;
    nx::utils::promise<void> m_itemCanBeUnlockedPromise;
    nx::utils::future<void> m_itemCanBeUnlockedFuture;

    void lockItemThreadFunc()
    {
        Items::ScopedLock lock(m_items, m_id);
        m_itemHasBeenLockedPromise.set_value();
        m_itemCanBeUnlockedFuture.wait();
    }
};

class QnGeneralAttributePool:
    public ::testing::Test
{
public:
    QnGeneralAttributePool():
        m_lockItemThread(&m_items, 1),
        m_lockAnotherItemThread(&m_items, 2)
    {
        m_resetHasFinishedFuture = m_resetHasFinishedPromise.get_future();

        m_lockItemThread.setValue(1);
        m_lockAnotherItemThread.setValue(2);
    }

    ~QnGeneralAttributePool()
    {
        if (m_resetItemsThread.joinable())
            m_resetItemsThread.join();
    }

protected:
    void givenLockedItem()
    {
        m_lockItemThread.start();
        m_lockItemThread.itemHasBeenLockedFuture().wait();
    }

    void whenResettingItemPoolConcurrently()
    {
        m_resetItemsThread = 
            nx::utils::thread(std::bind(&QnGeneralAttributePool::resetItemsThreadFunc, this));
    }

    void whenTriedToLockAnotherItem()
    {
        m_blockedInItemsClearPromise.get_future().wait();
        m_lockAnotherItemThread.start();
    }

    void thenResetShouldBlockUntilItemHasBeenUnlocked()
    {
        m_blockedInItemsClearPromise.get_future().wait();
        // Not very reliable, but statistically should be fine.
        ASSERT_EQ(
            std::future_status::timeout,
            m_resetHasFinishedFuture.wait_for(std::chrono::milliseconds(1)));
        m_lockItemThread.unlockItemAsync();
        m_resetHasFinishedFuture.wait();
    }

    void thenInterleavedLockShouldBlockUntilResetHasFinished()
    {
        // Not very reliable, but statistically should be fine.
        ASSERT_EQ(
            std::future_status::timeout,
            m_lockAnotherItemThread.itemHasBeenLockedFuture()
                .wait_for(std::chrono::milliseconds(1)));
        m_lockItemThread.unlockItemAsync();
        m_lockAnotherItemThread.unlockItemAsync();

        m_resetHasFinishedFuture.wait();

        m_lockAnotherItemThread.join();
        // Item has been re-created, so its value should be zero.
        ASSERT_EQ(0, m_lockAnotherItemThread.value());
    }

private:
    Items m_items;
    LockItemThread m_lockItemThread;
    LockItemThread m_lockAnotherItemThread;
    nx::utils::thread m_resetItemsThread;
    nx::utils::promise<void> m_blockedInItemsClearPromise;
    nx::utils::promise<void> m_resetHasFinishedPromise;
    nx::utils::future<void> m_resetHasFinishedFuture;

    void resetItemsThreadFunc()
    {
        m_items.clear([this]() { m_blockedInItemsClearPromise.set_value(); });
        m_resetHasFinishedPromise.set_value();
    }
};

TEST_F(QnGeneralAttributePool, concurrent_reset)
{
    givenLockedItem();
    whenResettingItemPoolConcurrently();
    thenResetShouldBlockUntilItemHasBeenUnlocked();
}

TEST_F(QnGeneralAttributePool, reset_takes_finite_time_to_execute)
{
    givenLockedItem();
    whenResettingItemPoolConcurrently();
    whenTriedToLockAnotherItem();
    thenInterleavedLockShouldBlockUntilResetHasFinished();
}

} // namespace test
