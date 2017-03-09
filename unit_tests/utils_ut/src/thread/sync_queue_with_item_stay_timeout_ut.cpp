#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

TEST(SyncQueueWithItemStayTimeout, basic)
{
    constexpr const auto timeout = std::chrono::milliseconds(500);

    std::deque<int> cancelledItems;

    nx::utils::SyncQueueWithItemStayTimeout<int> testQueue;
    testQueue.enableItemStayTimeoutEvent(
        timeout,
        [&cancelledItems](int item)
        {
            cancelledItems.push_back(item);
        });
    testQueue.push(1);
    testQueue.push(2);
    const auto item1 = testQueue.pop();
    ASSERT_TRUE(static_cast<bool>(item1));
    ASSERT_EQ(1, *item1);

    std::this_thread::sleep_for(2 * timeout);
    const auto item2 = testQueue.pop(timeout);
    ASSERT_FALSE(item2);

    ASSERT_EQ(1U, cancelledItems.size());
    ASSERT_EQ(2, cancelledItems[0]);
}
