// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/prometheus/registry.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <nx/sql/db_statistics_collector.h>
#include <nx/sql/detail/query_queue.h>

namespace nx::sql::test {

class DbStatisticsCollector: public ::testing::Test
{
public:
    DbStatisticsCollector(): m_statisticsCollector(m_period, m_queryQueue, &m_threadPoolSize) {}

protected:
    std::chrono::milliseconds statisticsAggregationPeriod() const { return m_period; }

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

    void addRandomRecord(
        QueryType queryType = QueryType::lookup, std::optional<DBResult> result = DBResultCode::ok)
    {
        using namespace std::chrono;

        const QueryExecutionTaskRecord queryInfo{
            .queryType = queryType,
            .result = result,
            .waitForExecutionDuration = milliseconds(nx::utils::random::number<int>(0, 100)),
            .executionDuration = milliseconds(nx::utils::random::number<int>(0, 100)),
        };
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

    void assertStatisticsIsCalculatedByLastRecords()
    {
        Statistics queryStatistics;
        std::chrono::milliseconds requestExecutionTimeTotal(0);
        std::chrono::milliseconds executionWaitTimeTotal(0);
        for (const auto& record: m_records)
        {
            if (record.result)
            {
                if (DBResultCode::ok == record.result->code)
                {
                    ++queryStatistics.requestsSucceeded;
                    if (QueryType::modification == record.queryType)
                        ++queryStatistics.modificationRequestsSucceeded;
                }
                else
                {
                    ++queryStatistics.requestsFailed;
                    if (QueryType::modification == record.queryType)
                        ++queryStatistics.modificationRequestsFailed;
                }
            }
            else
            {
                ++queryStatistics.requestsCancelled;
                if (QueryType::modification == record.queryType)
                    ++queryStatistics.modificationRequestsCancelled;
            }

            if (record.executionDuration)
            {
                calcTime(&queryStatistics.requestExecutionTimes,
                    *record.executionDuration,
                    &requestExecutionTimeTotal,
                    m_records.size());
            }
            calcTime(&queryStatistics.waitingForExecutionTimes,
                record.waitForExecutionDuration,
                &executionWaitTimeTotal,
                m_records.size());
        }

        assertEqual(queryStatistics, m_statisticsCollector.getStatistics());
    }

    int totalModificationRequests() const
    {
        return m_statisticsCollector.getStatistics().totalModificationRequests;
    }

    void assertWaitForExecutionTimeMinMaxAverageEqualTo(std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        assertTimeMinMaxAverageEqualTo(
            m_statisticsCollector.getStatistics().waitingForExecutionTimes,
            expectedMin,
            expectedMax,
            expectedAverage);
    }

    void assertExecutionTimeMinMaxAverageEqualTo(std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        assertTimeMinMaxAverageEqualTo(m_statisticsCollector.getStatistics().requestExecutionTimes,
            expectedMin,
            expectedMax,
            expectedAverage);
    }

    void assertQueryStatisticsEqual(const std::map<std::string, QueryStatistics>& expected)
    {
        const auto actual = m_statisticsCollector.getStatistics().queries;

        for (const auto& [query, expectedStatistics]: expected)
        {
            ASSERT_TRUE(actual.contains(query));
            auto actualStatistics = actual.find(query)->second;

            ASSERT_EQ(expectedStatistics.count, actualStatistics.count);
            assertEqual(
                expectedStatistics.requestExecutionTimes, actualStatistics.requestExecutionTimes);
        }
    }

private:
    detail::QueryQueue m_queryQueue;
    std::atomic<std::size_t> m_threadPoolSize{0};
    std::chrono::milliseconds m_period = std::chrono::milliseconds(100);
    StatisticsCollector m_statisticsCollector;
    nx::utils::test::ScopedTimeShift m_timeShift{nx::utils::test::ClockType::steady};
    std::deque<QueryExecutionTaskRecord> m_records;

    void assertTimeMinMaxAverageEqualTo(DurationStatistics statistics,
        std::chrono::milliseconds expectedMin,
        std::chrono::milliseconds expectedMax,
        std::chrono::milliseconds expectedAverage)
    {
        ASSERT_EQ(expectedMin, statistics.min);
        ASSERT_EQ(expectedMax, statistics.max);
        ASSERT_EQ(expectedAverage, statistics.average);
    }

    void calcTime(DurationStatistics* stats,
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
        ASSERT_EQ(expected.modificationRequestsSucceeded, actual.modificationRequestsSucceeded);
        ASSERT_EQ(expected.modificationRequestsFailed, actual.modificationRequestsFailed);
        ASSERT_EQ(expected.modificationRequestsCancelled, actual.modificationRequestsCancelled);
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
        std::min(one, two), std::max(one, two), (one + two) / 2);
}

TEST_F(DbStatisticsCollector, execution_time_min_max_average)
{
    const auto one = std::chrono::milliseconds(1);
    const auto two = std::chrono::milliseconds(9);

    recordQueryExecutionTaskWithExecutionTime(one);
    recordQueryExecutionTaskWithExecutionTime(two);

    assertExecutionTimeMinMaxAverageEqualTo(
        std::min(one, two), std::max(one, two), (one + two) / 2);
}

TEST_F(DbStatisticsCollector, expired_elements_are_removed)
{
    addRandomRecord(QueryType::modification);
    waitForStatisticsToExpire();
    addRandomRecord();
    addRandomRecord();
    assertStatisticsIsCalculatedByLastRecords();
    ASSERT_EQ(1, totalModificationRequests());
}

TEST_F(DbStatisticsCollector, modification_requests_are_counted)
{
    addRandomRecord(QueryType::lookup);
    addRandomRecord(QueryType::modification);
    addRandomRecord(QueryType::modification, DBResultCode::statementError);
    addRandomRecord(QueryType::modification, std::nullopt);
    assertStatisticsIsCalculatedByLastRecords();
    ASSERT_EQ(3, totalModificationRequests());
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

    QueryStatistics expectedQueryStatistics{
        .count = 2,
        .requestExecutionTimes = {.min = std::min(one, two),
            .max = std::max(one, two),
            .average = (one + two) / 2},
    };

    std::map<std::string, QueryStatistics> expected = {
        {queryOne, expectedQueryStatistics}, {queryTwo, expectedQueryStatistics}};

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
        .requestExecutionTimes = {.min = std::min(one, two),
            .max = std::max(one, two),
            .average = (one + two) / 2},
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
        .requestExecutionTimes = {.min = nine, .max = nine, .average = nine},
    };

    assertQueryStatisticsEqual({
        {queryOne, expectedQueryStatistics},
        {queryTwo, expectedQueryStatistics},
    });
}

//-------------------------------------------------------------------------------------------------
// Prometheus metrics

namespace {

constexpr char kQueryTasks[] = "nx_sql_query_tasks_total";
constexpr char kQueryDurationCount[] = "nx_sql_query_duration_seconds_count";
constexpr char kQueryDurationSum[] = "nx_sql_query_duration_seconds_sum";
constexpr char kQueueWaitCount[] = "nx_sql_queue_wait_seconds_count";
constexpr char kQueueWaitSum[] = "nx_sql_queue_wait_seconds_sum";
constexpr char kPendingQueries[] = "nx_sql_queue_pending_queries";
constexpr char kOldestQueryAge[] = "nx_sql_queue_oldest_query_age_seconds";
constexpr char kThreadPoolSize[] = "nx_sql_thread_pool_size";

/**
 * Value of the exposed sample named name whose label set contains every given "name=value"
 * fragment, or nullopt if no such sample was exposed. Matching by fragment keeps the tests
 * independent of the order the serializer prints labels in.
 */
std::optional<double> sampleValue(const std::string& serialized,
    const std::string& name,
    const std::vector<std::string>& labels = {})
{
    std::istringstream stream(serialized);
    for (std::string line; std::getline(stream, line);)
    {
        if (!line.starts_with(name))
            continue;

        const char afterName = line[name.size()];
        if (afterName != '{' && afterName != ' ')
            continue;

        if (std::any_of(labels.begin(),
                labels.end(),
                [&line](const auto& label) { return line.find(label) == std::string::npos; }))
        {
            continue;
        }

        return std::stod(line.substr(line.rfind(' ') + 1));
    }

    return std::nullopt;
}

class QueryExecutorStub: public detail::AbstractExecutor
{
public:
    virtual DBResult execute(AbstractDbConnection* const /*connection*/) override
    {
        return DBResultCode::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult /*errorCode*/) override {}

    virtual QueryType queryType() const override { return QueryType::lookup; }

    virtual void setOnBeforeDestruction(nx::MoveOnlyFunc<void()> /*handler*/) override {}

    virtual void setExternalTransaction(Transaction* /*transaction*/) override {}

    virtual std::string aggregationKey() const override { return std::string(); }
};

} // namespace

class DbStatisticsCollectorMetrics: public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds kPeriod = std::chrono::milliseconds(100);

    void recordTask(QueryType queryType,
        std::optional<DBResult> result,
        std::chrono::milliseconds waitForExecution = std::chrono::milliseconds::zero(),
        std::optional<std::chrono::milliseconds> execution = std::nullopt)
    {
        m_statisticsCollector.recordQueryExecutionTask(
            QueryExecutionTaskRecord{.queryType = queryType,
                .result = result,
                .waitForExecutionDuration = waitForExecution,
                .executionDuration = execution});
    }

    nx::prometheus::Registry m_registry{"nx_sql_ut", "test"};
    detail::QueryQueue m_queryQueue;
    std::atomic<std::size_t> m_threadPoolSize{0};
    StatisticsCollector m_statisticsCollector{
        kPeriod, m_queryQueue, &m_threadPoolSize, &m_registry};
};

TEST_F(DbStatisticsCollectorMetrics, no_registry_means_no_series)
{
    nx::prometheus::Registry registry{"nx_sql_ut", "test"};
    detail::QueryQueue queryQueue;
    std::atomic<std::size_t> threadPoolSize{0};
    StatisticsCollector collector(kPeriod, queryQueue, &threadPoolSize);

    collector.recordQueryExecutionTask(QueryExecutionTaskRecord{.queryType = QueryType::lookup,
        .result = DBResultCode::ok,
        .waitForExecutionDuration = std::chrono::milliseconds(1),
        .executionDuration = std::chrono::milliseconds(2)});

    EXPECT_EQ(registry.serialize().find("nx_sql_"), std::string::npos);
    EXPECT_EQ(collector.getStatistics().requestsSucceeded, 1);
}

TEST_F(DbStatisticsCollectorMetrics, task_counter_is_labelled_by_query_type_and_outcome)
{
    recordTask(QueryType::lookup, DBResultCode::ok);
    recordTask(QueryType::lookup, DBResultCode::ok);
    recordTask(QueryType::lookup, DBResultCode::statementError);
    recordTask(QueryType::modification, std::nullopt);

    const std::string serialized = m_registry.serialize();
    EXPECT_EQ(
        sampleValue(serialized, kQueryTasks, {"query_type=\"lookup\"", "outcome=\"ok\""}), 2.0);
    EXPECT_EQ(
        sampleValue(serialized, kQueryTasks, {"query_type=\"lookup\"", "outcome=\"failed\""}),
        1.0);
    EXPECT_EQ(
        sampleValue(
            serialized, kQueryTasks, {"query_type=\"modification\"", "outcome=\"cancelled\""}),
        1.0);
    // Pre-resolved in the constructor, so an outcome that never happened reads as zero.
    EXPECT_EQ(
        sampleValue(serialized, kQueryTasks, {"query_type=\"modification\"", "outcome=\"ok\""}),
        0.0);
}

TEST_F(DbStatisticsCollectorMetrics, durations_are_observed_in_seconds)
{
    recordTask(QueryType::modification,
        DBResultCode::ok,
        std::chrono::milliseconds(20),
        std::chrono::milliseconds(10));

    const std::string serialized = m_registry.serialize();
    const std::vector<std::string> labels{"query_type=\"modification\""};
    EXPECT_EQ(sampleValue(serialized, kQueryDurationCount, labels), 1.0);
    EXPECT_DOUBLE_EQ(*sampleValue(serialized, kQueryDurationSum, labels), 0.01);
    EXPECT_EQ(sampleValue(serialized, kQueueWaitCount, labels), 1.0);
    EXPECT_DOUBLE_EQ(*sampleValue(serialized, kQueueWaitSum, labels), 0.02);
}

TEST_F(DbStatisticsCollectorMetrics, a_task_that_never_ran_observes_no_duration)
{
    recordTask(QueryType::lookup, std::nullopt, std::chrono::milliseconds(20));

    const std::string serialized = m_registry.serialize();
    const std::vector<std::string> labels{"query_type=\"lookup\""};
    EXPECT_EQ(sampleValue(serialized, kQueryDurationCount, labels), 0.0);
    EXPECT_EQ(sampleValue(serialized, kQueueWaitCount, labels), 1.0);
}

TEST_F(DbStatisticsCollectorMetrics, every_query_type_has_a_duration_series_before_any_task)
{
    // The series are resolved at construction, so a rate() or histogram_quantile() over them is
    // never over an absent series, and observing costs no per-label-set lookup.
    const std::string serialized = m_registry.serialize();
    for (const auto* queryType: {"lookup", "modification"})
    {
        const std::vector<std::string> labels{std::string("query_type=\"") + queryType + '"'};
        EXPECT_EQ(sampleValue(serialized, kQueryDurationCount, labels), 0.0) << queryType;
        EXPECT_EQ(sampleValue(serialized, kQueueWaitCount, labels), 0.0) << queryType;
    }
}

TEST_F(DbStatisticsCollectorMetrics, queue_gauges_are_sampled_at_scrape_time)
{
    // Both change after the collector was registered, so a scrape reporting them proves the
    // values are pulled at serialize() time.
    m_threadPoolSize = 4;
    m_queryQueue.push(std::make_unique<QueryExecutorStub>());
    m_queryQueue.push(std::make_unique<QueryExecutorStub>());

    const std::string serialized = m_registry.serialize();
    EXPECT_EQ(sampleValue(serialized, kPendingQueries), 2.0);
    EXPECT_EQ(sampleValue(serialized, kThreadPoolSize), 4.0);
}

TEST_F(DbStatisticsCollectorMetrics, oldest_query_age_gauge_reports_the_head_query_wait)
{
    using namespace std::chrono;

    {
        // The queue stamps the enqueue time with the shiftable monotonic clock but ages it
        // against a raw steady_clock, so a backwards shift while pushing gives a known age.
        nx::utils::test::ScopedTimeShift shift(nx::utils::test::ClockType::steady, -5s);
        m_queryQueue.push(std::make_unique<QueryExecutorStub>());
    }
    // Only the main queue is aged, and this is the public way to drain the preliminary one.
    m_queryQueue.reportTimedOutQueries();

    const auto age = sampleValue(m_registry.serialize(), kOldestQueryAge);
    ASSERT_TRUE(age.has_value());
    EXPECT_GE(*age, 5.0);
    EXPECT_LT(*age, 6.0);
}

} // namespace nx::sql::test
