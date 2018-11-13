#include <gtest/gtest.h>

#include <nx/cloud/relaying/detail/statistics_calculator.h>
#include <nx/utils/random.h>

namespace nx {
namespace cloud {
namespace relaying {
namespace detail {
namespace test {

class StatisticsCalculator:
    public ::testing::Test
{
protected:
    detail::StatisticsCalculator m_statisticsCalculator;
    int m_expectedTotalConnectionCount = 0;

    void simulateRandomUsage()
    {
        m_expectedTotalConnectionCount = 0;

        const auto actionCount = nx::utils::random::number<int>(100, 1000);
        for (int i = 0; i < actionCount; ++i)
        {
            const auto action = nx::utils::random::number<int>(0, 2);
            switch (action)
            {
                case 0:
                    m_statisticsCalculator.connectionAccepted();
                    ++m_expectedTotalConnectionCount;
                    break;
                case 1:
                    m_statisticsCalculator.connectionUsed();
                    --m_expectedTotalConnectionCount;
                    break;
                case 2:
                    m_statisticsCalculator.connectionClosed();
                    --m_expectedTotalConnectionCount;
                    break;
            }
        }
    }
};

TEST_F(StatisticsCalculator, listeningServerCount)
{
    const auto val = nx::utils::random::number<int>();
    ASSERT_EQ(val, m_statisticsCalculator.calculateStatistics(val).listeningServerCount);
}

TEST_F(StatisticsCalculator, connectionCount)
{
    simulateRandomUsage();
    ASSERT_EQ(
        m_expectedTotalConnectionCount,
        m_statisticsCalculator.calculateStatistics(1).connectionCount);
}

} // namespace test
} // namespace detail
} // namespace relaying
} // namespace cloud
} // namespace nx
