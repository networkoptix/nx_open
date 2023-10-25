// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>

#include <nx/reflect/instrument.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "types.h"

namespace nx::sql {

namespace detail { class QueryQueue; }

//-------------------------------------------------------------------------------------------------

struct QueryExecutionInfo
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
        const detail::QueryQueue& queryQueue);

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
    const detail::QueryQueue& m_queryQueue;
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
