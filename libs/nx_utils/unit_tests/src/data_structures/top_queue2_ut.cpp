#include <gtest/gtest.h>

#include <nx/utils/data_structures/top_queue2.h>

namespace nx {
namespace utils {
namespace test {

class TopQueue2:
    public ::testing::Test
{
protected:
    template<
        typename GenerateValueFunc,
        const int& (*Comp)(const int&, const int&) = std::min<int>,
        int bottomElement = std::numeric_limits<int>::max()>
    void testTopQueue(GenerateValueFunc func)
    {
        constexpr int elementCount = 1000;
        constexpr int maxQueueSize = 100;

        utils::TopQueue2<int, Comp> topQueue(bottomElement);
        std::deque<int> checkQueue;

        for (int i = 0; i < elementCount; ++i)
        {
            const auto val = func();
            topQueue.push_back(val);
            checkQueue.push_back(val);

            if (checkQueue.size() > maxQueueSize)
            {
                topQueue.pop_front();
                checkQueue.pop_front();
            }

            auto expectedResult = bottomElement;
            for (const auto& value: checkQueue)
                expectedResult = Comp(expectedResult, value);

            ASSERT_EQ(expectedResult, topQueue.top());
        }
    }
};

TEST_F(TopQueue2, random_data)
{
    testTopQueue([]() { return rand(); });
}

TEST_F(TopQueue2, max_queue_random_data)
{
    auto generateFunc = []() { return rand(); };
    testTopQueue<decltype(generateFunc),
        std::max<int>,
        std::numeric_limits<int>::min()>(generateFunc);
}

TEST_F(TopQueue2, min_queue_random_data)
{
    auto generateFunc = []() { return rand(); };
    testTopQueue<decltype(generateFunc),
        std::min<int>,
        std::numeric_limits<int>::max()>(generateFunc);
}

TEST_F(TopQueue2, ascending_data)
{
    int prevValue = 0;
    testTopQueue([&prevValue]() { return prevValue++; });
}

TEST_F(TopQueue2, descending_data)
{
    int prevValue = 100000000;
    testTopQueue([&prevValue]() { return prevValue--; });
}

TEST_F(TopQueue2, updateData)
{
    QueueWithMax<int> topQueue;
    topQueue.push_back(3);
    topQueue.push_back(7);
    topQueue.push_back(5);

    ASSERT_EQ(7, topQueue.top());
    topQueue.last() = 10;
    ASSERT_EQ(7, topQueue.top());
    topQueue.update();

    while (!topQueue.empty())
    {
        ASSERT_EQ(10, topQueue.top());
        topQueue.pop_front();
    }
}

} // namespace test
} // namespace utils
} // namespace nx
