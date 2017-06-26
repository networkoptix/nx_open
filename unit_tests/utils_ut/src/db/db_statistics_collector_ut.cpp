#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <nx/utils/db/db_statistics_collector.h>

namespace nx {
namespace utils {
namespace db {
namespace test {

class DbStatisticsCollector:
    public ::testing::Test
{
public:
    DbStatisticsCollector():
        m_statisticsCollector(std::chrono::milliseconds(100))
    {
    }

protected:
    void recordRequestWithWaitForExecutionTime(std::chrono::milliseconds val)
    {
        QueryExecutionInfo queryInfo;
        queryInfo.waitForExecutionDuration = val;
        m_statisticsCollector.recordQuery(queryInfo);
    }

    void recordRequestWithExecutionTime(std::chrono::milliseconds val)
    {
        QueryExecutionInfo queryInfo;
        queryInfo.executionDuration = val;
        m_statisticsCollector.recordQuery(queryInfo);
    }

    void addRandomRecord()
    {
        using namespace std::chrono;

        QueryExecutionInfo queryInfo;
        queryInfo.executionDuration = milliseconds(nx::utils::random::number<int>(0, 100));
        queryInfo.waitForExecutionDuration = milliseconds(nx::utils::random::number<int>(0, 100));
        queryInfo.result = DBResult::ok;
        m_statisticsCollector.recordQuery(queryInfo);
        m_records.push_back(queryInfo);
    }

    void waitForStatisticsToExpire()
    {
        m_timeShifts.push_back(
            nx::utils::test::ScopedTimeShift(
                nx::utils::test::ClockType::steady,
                m_statisticsCollector.aggregationPeriod()));
        m_records.clear();
    }

    void assertStatisticsIsCalculatedByLastRecords(int /*count*/)
    {
        QueryStatistics queryStatistics;
        std::chrono::milliseconds requestExecutionTimeTotal(0);
        std::chrono::milliseconds executionWaitTimeTotal(0);
        for (const auto& record: m_records)
        {
            if (record.result)
            {
                if (*record.result == DBResult::ok)
                    ++queryStatistics.requestsSucceeded;
                else
                    ++queryStatistics.requestsFailed;
            }
            else
            {
                ++queryStatistics.requestsCancelled;
            }

            if (record.executionDuration)
            {
                calcTime(
                    &queryStatistics.requestExecutionTimes,
                    *record.executionDuration,
                    &requestExecutionTimeTotal,
                    m_records.size());
            }
            calcTime(
                &queryStatistics.waitingForExecutionTimes,
                record.waitForExecutionDuration,
                &executionWaitTimeTotal,
                m_records.size());
        }

        assertEqual(queryStatistics, m_statisticsCollector.getQueryStatistics());
    }

    void assertWaitForExecutionTimeMinMaxAverageEqualTo(
        std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        assertTimeMinMaxAverageEqualTo(
            m_statisticsCollector.getQueryStatistics().waitingForExecutionTimes,
            expectedMin,
            expectedMax,
            expectedAverage);
    }

    void assertExecutionTimeMinMaxAverageEqualTo(
        std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        assertTimeMinMaxAverageEqualTo(
            m_statisticsCollector.getQueryStatistics().requestExecutionTimes,
            expectedMin,
            expectedMax,
            expectedAverage);
    }

private:
    StatisticsCollector m_statisticsCollector;
    std::deque<nx::utils::test::ScopedTimeShift> m_timeShifts;
    std::deque<QueryExecutionInfo> m_records;

    void assertTimeMinMaxAverageEqualTo(
        DurationStatistics statistics,
        std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        ASSERT_EQ(expectedMin, statistics.min);
        ASSERT_EQ(expectedMax, statistics.max);
        ASSERT_EQ(expectedAverage, statistics.average);
    }

    void calcTime(
        DurationStatistics* stats,
        std::chrono::milliseconds value,
        std::chrono::milliseconds* total,
        std::size_t count)
    {
        stats->min = std::min(stats->min, value);
        stats->max = std::max(stats->max, value);
        *total += value;
        stats->average = *total / count;
    }

    void assertEqual(const QueryStatistics& expected, const QueryStatistics& actual)
    {
        ASSERT_EQ(expected.requestsCancelled, actual.requestsCancelled);
        ASSERT_EQ(expected.requestsFailed, actual.requestsFailed);
        ASSERT_EQ(expected.requestsSucceeded, actual.requestsSucceeded);
        assertEqual(expected.requestExecutionTimes, actual.requestExecutionTimes);
        assertEqual(expected.waitingForExecutionTimes, actual.waitingForExecutionTimes);
    }

    void assertEqual(const DurationStatistics& expected, const DurationStatistics& actual)
    {
        ASSERT_EQ(expected.min, actual.min);
        ASSERT_EQ(expected.max, actual.max);
        ASSERT_EQ(expected.average, actual.average);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases

TEST_F(DbStatisticsCollector, wait_for_execution_time_min_max_average)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    recordRequestWithWaitForExecutionTime(one);
    recordRequestWithWaitForExecutionTime(two);
    
    assertWaitForExecutionTimeMinMaxAverageEqualTo(
        std::min(one, two),
        std::max(one, two),
        (one + two) / 2);
}

TEST_F(DbStatisticsCollector, execution_time_min_max_average)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    recordRequestWithExecutionTime(one);
    recordRequestWithExecutionTime(two);

    assertExecutionTimeMinMaxAverageEqualTo(
        std::min(one, two),
        std::max(one, two),
        (one + two) / 2);
}

TEST_F(DbStatisticsCollector, expired_elements_are_removed)
{
    addRandomRecord();
    waitForStatisticsToExpire();
    addRandomRecord();
    addRandomRecord();
    assertStatisticsIsCalculatedByLastRecords(2);
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
