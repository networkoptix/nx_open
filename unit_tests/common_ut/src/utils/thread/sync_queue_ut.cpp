#include <gtest/gtest.h>

#include <thread>
#include <utils/thread/sync_queue.h>

namespace nx {
namespace test {

class SyncQueueTest
:
    public ::testing::Test
{
protected:
    std::thread push(
        std::vector<int> values,
        std::chrono::milliseconds delay = std::chrono::milliseconds(0))
    {
        return std::thread(
            [this, values = std::move(values), delay]()
            {
                for (const auto& value: values)
                {
                    if (delay.count())
                        std::this_thread::sleep_for(delay);

                    queue.push(value);
                }
            });
    }

    int popSum(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
    {
        int total(0);
        while (auto value = queue.pop(timeout))
            total += *value;

        return total;
    }

    void TearDown() override
    {
        ASSERT_TRUE(queue.isEmpty());
    }

    SyncQueue<int> queue;
};

TEST_F(SyncQueueTest, SinglePushPop)
{
    auto pusher = push({1, 2, 3, 4, 5});
    ASSERT_EQ(popSum(), 15);
    pusher.join();
}

TEST_F(SyncQueueTest, MultiPushPop)
{
    auto pusher1 = push({1, 2, 3, 4, 5});
    auto pusher2 = push({5, 5, 5});
    auto pusher3 = push({3, 3, 3, 3});

    ASSERT_EQ(popSum(), 42);

    pusher1.join();
    pusher2.join();
    pusher3.join();
}

TEST_F(SyncQueueTest, TimedPop)
{
    ASSERT_FALSE(queue.pop(std::chrono::milliseconds(100)));
    auto pusher = push({1, 2}, std::chrono::milliseconds(500));

    ASSERT_FALSE(queue.pop(std::chrono::milliseconds(100))); // too early
    ASSERT_EQ(*queue.pop(std::chrono::milliseconds(500)), 1); // just in time

    ASSERT_FALSE(queue.pop(std::chrono::milliseconds(100))); // too early again
    ASSERT_EQ(*queue.pop(std::chrono::milliseconds(500)), 2); // just in time

    pusher.join();
}

} // namespace test
} // namespace nx
