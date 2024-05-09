// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <nx/sql/db_statistics_collector.h>
#include <nx/sql/detail/query_queue.h>

namespace nx::sql::test {

class DbStatisticsCollector:
    public ::testing::Test
{
public:
    DbStatisticsCollector():
        m_statisticsCollector(m_period, m_queryQueue, &m_threadPoolSize)
    {
    }

protected:
    std::chrono::milliseconds statisticsAggregationPeriod() const
    {
        return m_period;
    }

    void recordQueryExecutionTaskWithWaitForExecutionTime(std::chrono::milliseconds val)
    {
        QueryExecutionTaskRecord queryInfo;
        queryInfo.waitForExecutionDuration = val;
        m_statisticsCollector.recordQueryExecutionTask(queryInfo);
    }

    void recordQueryExecutionTaskWithExecutionTime(std::chrono::milliseconds val)
    {
        QueryExecutionTaskRecord queryInfo;
        queryInfo.executionDuration = val;
        m_statisticsCollector.recordQueryExecutionTask(queryInfo);
    }

    void recordQueryWithExecutionTime(std::string query, std::chrono::milliseconds val)
    {
        m_statisticsCollector.recordQuery(std::move(query), val);
    }

    void addRandomRecord()
    {
        using namespace std::chrono;

        QueryExecutionTaskRecord queryInfo;
        queryInfo.executionDuration = milliseconds(nx::utils::random::number<int>(0, 100));
        queryInfo.waitForExecutionDuration = milliseconds(nx::utils::random::number<int>(0, 100));
        queryInfo.result = DBResultCode::ok;
        m_statisticsCollector.recordQueryExecutionTask(queryInfo);
        m_records.push_back(queryInfo);
    }

    void waitForStatisticsToExpire()
    {
        // sql statistics use SumPerPeriod, which calculates using subperiods, the most recent of
        // which is actually in the future. So, time needs to advance by the period + one extra
        // subperiod.
        waitForPeriod(m_period + m_period / 20 + std::chrono::milliseconds(1));
        m_records.clear();
    }

    void waitForPeriod(std::chrono::milliseconds timeShift)
    {
        m_timeShift.applyRelativeShift(timeShift);
    }

    void assertStatisticsIsCalculatedByLastRecords(int /*count*/)
    {
        Statistics queryStatistics;
        std::chrono::milliseconds requestExecutionTimeTotal(0);
        std::chrono::milliseconds executionWaitTimeTotal(0);
        for (const auto& record: m_records)
        {
            if (record.result)
            {
                if (*record.result == DBResultCode::ok)
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

        assertEqual(queryStatistics, m_statisticsCollector.getStatistics());
    }

    void assertWaitForExecutionTimeMinMaxAverageEqualTo(
        std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        assertTimeMinMaxAverageEqualTo(
            m_statisticsCollector.getStatistics().waitingForExecutionTimes,
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
            m_statisticsCollector.getStatistics().requestExecutionTimes,
            expectedMin,
            expectedMax,
            expectedAverage);
    }

    void assertQueryStatisticsEqual(const std::map<std::string, QueryStatistics>& expected)
    {
        const auto actual = m_statisticsCollector.getStatistics().queries;

        for (const auto& [query, expectedStatistics] : expected)
        {
            ASSERT_TRUE(actual.contains(query));
            auto actualStatistics = actual.find(query)->second;

            ASSERT_EQ(expectedStatistics.count, actualStatistics.count);
            assertEqual(
                expectedStatistics.requestExecutionTimes,
                actualStatistics.requestExecutionTimes);
        }
    }

private:
    detail::QueryQueue m_queryQueue;
    std::atomic<std::size_t> m_threadPoolSize{0};
    std::chrono::milliseconds m_period = std::chrono::milliseconds(100);
    StatisticsCollector m_statisticsCollector;
    nx::utils::test::ScopedTimeShift m_timeShift{
        nx::utils::test::ClockType::steady};
    std::deque<QueryExecutionTaskRecord> m_records;

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

    void assertEqual(const Statistics& expected, const Statistics& actual)
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

    recordQueryExecutionTaskWithWaitForExecutionTime(one);
    recordQueryExecutionTaskWithWaitForExecutionTime(two);

    assertWaitForExecutionTimeMinMaxAverageEqualTo(
        std::min(one, two),
        std::max(one, two),
        (one + two) / 2);
}

TEST_F(DbStatisticsCollector, execution_time_min_max_average)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    recordQueryExecutionTaskWithExecutionTime(one);
    recordQueryExecutionTaskWithExecutionTime(two);

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

TEST_F(DbStatisticsCollector, queries_statistics)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    const std::string queryOne{"SELECT * FROM table"};
    const std::string queryTwo{"SELECT * FROM table_two"};

    recordQueryWithExecutionTime(queryOne, one);
    recordQueryWithExecutionTime(queryOne, two);

    recordQueryWithExecutionTime(queryTwo, one);
    recordQueryWithExecutionTime(queryTwo, two);

    QueryStatistics expectedQueryStatistics {
        .count = 2,
        .requestExecutionTimes = {
            .min = std::min(one, two),
            .max = std::max(one, two),
            .average = (one + two) / 2
        },
    };

    std::map<std::string, QueryStatistics> expected = {
        {queryOne, expectedQueryStatistics},
        {queryTwo, expectedQueryStatistics}
    };

    assertQueryStatisticsEqual(expected);
}

TEST_F(DbStatisticsCollector, queries_expired_values)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    const std::string queryOne{"SELECT * FROM table"};
    const std::string queryTwo{"SELECT * FROM table_two"};

    recordQueryWithExecutionTime(queryOne, one);
    recordQueryWithExecutionTime(queryOne, two);

    recordQueryWithExecutionTime(queryTwo, one);
    recordQueryWithExecutionTime(queryTwo, two);

    waitForStatisticsToExpire();

    assertQueryStatisticsEqual({});
}

TEST_F(DbStatisticsCollector, queries_partially_expired_values_one)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    const std::string queryOne{"SELECT * FROM table"};
    const std::string queryTwo{"SELECT * FROM table_two"};

    recordQueryWithExecutionTime(queryOne, one);
    recordQueryWithExecutionTime(queryOne, two);

    waitForPeriod(statisticsAggregationPeriod() / 2);

    recordQueryWithExecutionTime(queryTwo, one);
    recordQueryWithExecutionTime(queryTwo, two);

    waitForPeriod(statisticsAggregationPeriod() / 2);

    QueryStatistics expectedQueryStatistics{
        .count = 2,
        .requestExecutionTimes = {
            .min = std::min(one, two),
            .max = std::max(one, two),
            .average = (one + two) / 2
        },
    };

    assertQueryStatisticsEqual({{queryTwo, expectedQueryStatistics}});
}

TEST_F(DbStatisticsCollector, queries_partially_expired_values_two)
{
    const auto one = std::chrono::milliseconds(1);
    const auto nine = std::chrono::milliseconds(9);

    const std::string queryOne{"SELECT * FROM table"};
    const std::string queryTwo{"SELECT * FROM table_two"};

    recordQueryWithExecutionTime(queryOne, one);
    recordQueryWithExecutionTime(queryTwo, one);

    waitForPeriod(statisticsAggregationPeriod() / 2);

    recordQueryWithExecutionTime(queryOne, nine);
    recordQueryWithExecutionTime(queryTwo, nine);

    waitForPeriod(statisticsAggregationPeriod() / 2 + statisticsAggregationPeriod() / 20);

    QueryStatistics expectedQueryStatistics{
        .count = 1,
        .requestExecutionTimes = {
            .min = nine,
            .max = nine,
            .average = nine
        },
    };

    assertQueryStatisticsEqual({
        {queryOne, expectedQueryStatistics},
        {queryTwo, expectedQueryStatistics},
        });
}

} // namespace nx::sql::test
