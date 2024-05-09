// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <unordered_map>

#include <nx/reflect/instrument.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/math/average_per_period.h>
#include <nx/utils/math/max_per_period.h>
#include <nx/utils/math/sum_per_period.h>
#include <nx/utils/math/summary_statistics_per_period.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "types.h"

namespace nx::sql {

namespace detail { class QueryQueue; }

//-------------------------------------------------------------------------------------------------

/**
 * Record of one query execution task, i.e. one call to executeSelect() or executeUpdate().
 */
struct QueryExecutionTaskRecord
{
    std::optional<DBResult> result;
    std::chrono::milliseconds waitForExecutionDuration = std::chrono::milliseconds::zero();
    std::optional<std::chrono::milliseconds> executionDuration;
};

class NX_SQL_API StatisticsCollector
{
public:
    StatisticsCollector(
        std::chrono::milliseconds period,
        const detail::QueryQueue& queryQueue,
        std::atomic<std::size_t>* dbThreadPoolSize);

    /**
     * Record statistics about how long it took to perform a query execution task.
     * NOTE: a single task may include multiple queries and a transaction commit.
     */
    void recordQueryExecutionTask(QueryExecutionTaskRecord record);

    /**
     * Record statistics about a specific query immediately after query execution.
     */
    void recordQuery(std::string query, std::chrono::milliseconds executionTime);
    Statistics getStatistics() const;

    std::chrono::milliseconds aggregationPeriod() const;

    void clearStatistics();

private:
    struct QueryExecutionTaskContext
    {
        nx::utils::math::SumPerPeriod<int> successfulRequestsCounter;
        nx::utils::math::SumPerPeriod<int> failedRequestsCounter;
        nx::utils::math::SumPerPeriod<int> cancelledRequestsCounter;

        nx::utils::math::SummaryStatisticsPerPeriod<std::chrono::milliseconds>
            taskExecutionTimeCounter;

        nx::utils::math::SummaryStatisticsPerPeriod<std::chrono::milliseconds>
            tasksWaitingForExecutionCounter;

        QueryExecutionTaskContext(std::chrono::milliseconds period);

        void reset();
    };

    struct SingleQueryStatisticsContext
    {
        nx::utils::math::SummaryStatisticsPerPeriod<std::chrono::milliseconds>
            statisticsCalculator;
        nx::utils::math::SumPerPeriod<int> frequencyCounter;
        DurationStatistics durationStatistics;

        SingleQueryStatisticsContext(std::chrono::milliseconds period);

        void reset();
    };

    const std::chrono::milliseconds m_period;
    const detail::QueryQueue& m_queryQueue;
    std::atomic<std::size_t>* m_dbThreadPoolSize = nullptr;
    mutable nx::Mutex m_mutex;
    QueryExecutionTaskContext m_queryExecutionTaskStatistics;
    std::unordered_map<std::string /*query */, SingleQueryStatisticsContext> m_queryStatistics;

    DurationStatistics getDurationStatistics(
        nx::utils::math::SummaryStatisticsPerPeriod<std::chrono::milliseconds>* calculator);

    std::map<std::string, QueryStatistics> getQueryStatistics(const nx::MutexLocker&) const;
};

} // namespace nx::sql
