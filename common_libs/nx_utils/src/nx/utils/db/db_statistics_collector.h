#pragma once

#include <chrono>
#include <deque>

#include <boost/optional.hpp>

#include <nx/utils/thread/mutex.h>

#include "types.h"

namespace nx {
namespace utils {
namespace db {

struct NX_UTILS_API QueryExecutionInfo
{
    boost::optional<DBResult> result;
    std::chrono::milliseconds waitForExecutionDuration;
    boost::optional<std::chrono::milliseconds> executionDuration;

    QueryExecutionInfo();
};

struct NX_UTILS_API DurationStatistics
{
    std::chrono::milliseconds min;
    std::chrono::milliseconds max;
    std::chrono::milliseconds average;

    DurationStatistics();
};

struct NX_UTILS_API QueryStatistics
{
    std::chrono::milliseconds statisticalPeriod;
    int requestsSucceeded;
    int requestsFailed;
    int requestsCancelled;
    DurationStatistics requestExecutionTimes;
    DurationStatistics waitingForExecutionTimes;

    QueryStatistics();
};

class NX_UTILS_API StatisticsCollector
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
        DurationStatistics* result;
        std::chrono::milliseconds currentSum;
        std::size_t count;
        bool recalcMinMax;

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
    mutable QnMutex m_mutex;
    QueryStatistics m_currentStatistics;
    DurationStatisticsCalculationContext m_requestExecutionTimesCalculationContext;
    DurationStatisticsCalculationContext m_waitingForExecutionTimesCalculationContext;

    void updateStatisticsWithNewValue(const QueryExecutionInfo& queryStatistics);
    void addValue(
        DurationStatisticsCalculationContext* calculationContext,
        std::chrono::milliseconds value);
    void removeExpiredRecords(const QnMutexLockerBase& lock);
    void removeValueFromStatistics(const QueryExecutionInfo& queryStatistics);
    void removeValue(
        DurationStatisticsCalculationContext* calculationContext,
        std::chrono::milliseconds value);
    void recalcIfNeeded();
    void updateMinMax(
        DurationStatistics* result,
        std::chrono::milliseconds value);
};

} // namespace db
} // namespace utils
} // namespace nx
