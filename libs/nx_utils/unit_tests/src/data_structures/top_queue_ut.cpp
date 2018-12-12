#include <gtest/gtest.h>

#include <nx/utils/data_structures/top_queue.h>

namespace nx {
namespace utils {
namespace test {

class TopQueue:
    public ::testing::Test
{
protected:
    template<
        typename GenerateValueFunc,
        typename Comp = std::less<int>
    >
    void testTopQueue(GenerateValueFunc func)
    {
        constexpr int elementCount = 1000;
        constexpr int maxQueueSize = 100;

        utils::TopQueue<int, Comp> topQueue;
        std::deque<int> checkQueue;

        for (int i = 0; i < elementCount; ++i)
        {
            const auto val = func();
            topQueue.push(val);
            checkQueue.push_back(val);

            if (checkQueue.size() > maxQueueSize)
            {
                topQueue.pop();
                checkQueue.pop_front();
            }

            ASSERT_EQ(
                topQueue.top(),
                *std::min_element(checkQueue.begin(), checkQueue.end(), Comp()));
        }
    }
};

TEST_F(TopQueue, random_data)
{
    testTopQueue([]() { return rand(); });
}

TEST_F(TopQueue, max_queue_random_data)
{
    auto generateFunc = []() { return rand(); };
    testTopQueue<decltype(generateFunc), std::greater<int>>(generateFunc);
}

TEST_F(TopQueue, ascending_data)
{
    int prevValue = 0;
    testTopQueue([&prevValue]() { return prevValue++; });
}

TEST_F(TopQueue, descending_data)
{
    int prevValue = 100000000;
    testTopQueue([&prevValue]() { return prevValue--; });
}

} // namespace test
} // namespace utils
} // namespace nx
