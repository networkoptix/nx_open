// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "types.h"

namespace nx::sql {

struct NX_SQL_API QueryExecutionInfo
{
    std::optional<DBResult> result;
    std::chrono::milliseconds waitForExecutionDuration = std::chrono::milliseconds::zero();
    std::optional<std::chrono::milliseconds> executionDuration;
};

struct NX_SQL_API DurationStatistics
{
    std::chrono::milliseconds min = std::chrono::milliseconds::max();
    std::chrono::milliseconds max = std::chrono::milliseconds::min();
    std::chrono::milliseconds average = std::chrono::milliseconds::zero();
};

#define DurationStatistics_sql_Fields (min)(max)(average)
QN_FUSION_DECLARE_FUNCTIONS(DurationStatistics, (json), NX_SQL_API)

NX_REFLECTION_INSTRUMENT(DurationStatistics, DurationStatistics_sql_Fields)

struct NX_SQL_API QueryStatistics
{
    std::chrono::milliseconds statisticalPeriod;
    int requestsSucceeded = 0;
    int requestsFailed = 0;
    int requestsCancelled = 0;
    DurationStatistics requestExecutionTimes;
    DurationStatistics waitingForExecutionTimes;
};

#define QueryStatistics_sql_Fields (statisticalPeriod)(requestsSucceeded)(requestsFailed) \
    (requestsCancelled)(requestExecutionTimes)(waitingForExecutionTimes)
QN_FUSION_DECLARE_FUNCTIONS(QueryStatistics, (json), NX_SQL_API)

NX_REFLECTION_INSTRUMENT(QueryStatistics, QueryStatistics_sql_Fields)

//-------------------------------------------------------------------------------------------------

class NX_SQL_API StatisticsCollector
{
public:
    StatisticsCollector(std::chrono::milliseconds period);

    void recordQuery(QueryExecutionInfo queryStatistics);
    QueryStatistics getQueryStatistics() const;

    std::chrono::milliseconds aggregationPeriod() const;

    void clearStatistics();

private:
    struct DurationStatisticsCalculationContext
    {
        DurationStatistics* result = nullptr;
        std::chrono::milliseconds currentSum = std::chrono::milliseconds::zero();
        std::size_t count = 0;
        bool recalcMinMax = false;

        DurationStatisticsCalculationContext(DurationStatistics* result);
    };

    struct StatisticsRecordContext
    {
        QueryExecutionInfo data;
        std::chrono::steady_clock::time_point timestamp;

        StatisticsRecordContext(QueryExecutionInfo data);
    };

    const std::chrono::milliseconds m_period;
    std::deque<StatisticsRecordContext> m_recordQueue;
    mutable nx::Mutex m_mutex;
    QueryStatistics m_currentStatistics;
    nx::utils::ElapsedTimer m_cleanupTimeout;
    DurationStatisticsCalculationContext m_requestExecutionTimesCalculationContext;
    DurationStatisticsCalculationContext m_waitingForExecutionTimesCalculationContext;

    void updateStatisticsWithNewValue(const QueryExecutionInfo& queryStatistics);
    void addValue(
        DurationStatisticsCalculationContext* calculationContext,
        std::chrono::milliseconds value);
    void removeExpiredRecords(const nx::Locker<nx::Mutex>& lock);
    void removeValueFromStatistics(const QueryExecutionInfo& queryStatistics);
    void removeValue(
        DurationStatisticsCalculationContext* calculationContext,
        std::chrono::milliseconds value);
    void recalcIfNeeded();
    void updateMinMax(
        DurationStatistics* result,
        std::chrono::milliseconds value);
};

} // namespace nx::sql
