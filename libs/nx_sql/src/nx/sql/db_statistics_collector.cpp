// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_statistics_collector.h"

#include <nx/utils/time.h>

#include "detail/query_queue.h"

namespace nx::sql {

StatisticsCollector::QueryExecutionTaskContext::QueryExecutionTaskContext(std::chrono::milliseconds period)
    :
    successfulRequestsCounter(period),
    failedRequestsCounter(period),
    cancelledRequestsCounter(period),
    taskExecutionTimeCounter(period),
    tasksWaitingForExecutionCounter(period)
{}

void StatisticsCollector::QueryExecutionTaskContext::reset()
{
    successfulRequestsCounter.reset();
    failedRequestsCounter.reset();
    cancelledRequestsCounter.reset();
    taskExecutionTimeCounter.reset();
    tasksWaitingForExecutionCounter.reset();
}

StatisticsCollector::SingleQueryStatisticsContext::SingleQueryStatisticsContext(
    std::chrono::milliseconds period)
    :
    statisticsCalculator(period),
    frequencyCounter(period)
{}

void StatisticsCollector::SingleQueryStatisticsContext::reset()
{
    statisticsCalculator.reset();
    frequencyCounter.reset();
    durationStatistics = {};
}

StatisticsCollector::StatisticsCollector(
    std::chrono::milliseconds period,
    const detail::QueryQueue& queryQueue,
    std::atomic<std::size_t>* dbThreadPoolSize)
    :
    m_period(period),
    m_queryQueue(queryQueue),
    m_dbThreadPoolSize(dbThreadPoolSize),
    m_queryExecutionTaskStatistics(m_period)
{
}

void StatisticsCollector::recordQueryExecutionTask(QueryExecutionTaskRecord record)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (record.executionDuration)
        m_queryExecutionTaskStatistics.taskExecutionTimeCounter.add(*record.executionDuration);

    m_queryExecutionTaskStatistics.tasksWaitingForExecutionCounter.add(record.waitForExecutionDuration);

    if (!record.result)
    {
        m_queryExecutionTaskStatistics.cancelledRequestsCounter.add(1);
        return;
    }

    if (record.result == DBResultCode::ok)
        m_queryExecutionTaskStatistics.successfulRequestsCounter.add(1);
    else
        m_queryExecutionTaskStatistics.failedRequestsCounter.add(1);
}

void StatisticsCollector::recordQuery(
    std::string query,
    std::chrono::milliseconds executionTime)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto& queryStatistics = m_queryStatistics.emplace(std::move(query), m_period).first->second;
    queryStatistics.frequencyCounter.add(1);
    queryStatistics.statisticsCalculator.add(executionTime);
}

Statistics StatisticsCollector::getStatistics() const
{
    auto self = const_cast<StatisticsCollector*>(this);

    NX_MUTEX_LOCKER lock(&m_mutex);

    return Statistics
    {
        .statisticalPeriod = m_period,
        .requestsSucceeded =
            self->m_queryExecutionTaskStatistics.successfulRequestsCounter.getSumPerLastPeriod(),
        .requestsFailed =
            self->m_queryExecutionTaskStatistics.failedRequestsCounter.getSumPerLastPeriod(),
        .requestsCancelled =
            self->m_queryExecutionTaskStatistics.cancelledRequestsCounter.getSumPerLastPeriod(),
        .dbThreadPoolSize = m_dbThreadPoolSize->load(),
        .requestExecutionTimes =
            self->getDurationStatistics(&self->m_queryExecutionTaskStatistics.taskExecutionTimeCounter),
        .waitingForExecutionTimes =
            self->getDurationStatistics(&self->m_queryExecutionTaskStatistics.tasksWaitingForExecutionCounter),
        .queryQueue = m_queryQueue.stats(),
        .queries = getQueryStatistics(lock),
    };
}

std::chrono::milliseconds StatisticsCollector::aggregationPeriod() const
{
    return m_period;
}

void StatisticsCollector::clearStatistics()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_queryExecutionTaskStatistics.reset();

    for (auto& query : m_queryStatistics)
        query.second.reset();
}

DurationStatistics StatisticsCollector::getDurationStatistics(
    nx::utils::math::SummaryStatisticsPerPeriod<std::chrono::milliseconds>* calculator)
{
    const auto summaryStatistics = calculator->summaryStatisticsPerLastPeriod();
    return DurationStatistics{
        .min = summaryStatistics.min,
        .max = summaryStatistics.max,
        .average = summaryStatistics.average
    };
}

std::map<std::string, QueryStatistics> StatisticsCollector::getQueryStatistics(const nx::MutexLocker& /*lock*/) const
{
    static constexpr int kQueryCount = 5;

    std::multimap<int, decltype(m_queryStatistics)::const_iterator> queriesByCount;
    for (auto it = m_queryStatistics.begin(); it != m_queryStatistics.end(); ++it)
        queriesByCount.emplace(it->second.frequencyCounter.getSumPerLastPeriod(), it);

    std::map<std::string, QueryStatistics> result;

    int i = 0;
    for (auto it = queriesByCount.begin();
        i < kQueryCount && it != queriesByCount.end();
        ++i, ++it)
    {
        auto& queryStatisticsResult = result[it->second->first];
        auto& queryStatisticsCtx = it->second->second;

        const auto summaryStatistics =
            queryStatisticsCtx.statisticsCalculator.summaryStatisticsPerLastPeriod();

        queryStatisticsResult.count = it->first;
        queryStatisticsResult.requestExecutionTimes.average = summaryStatistics.average;
        queryStatisticsResult.requestExecutionTimes.min = summaryStatistics.min;
        queryStatisticsResult.requestExecutionTimes.max = summaryStatistics.max;
    }

    return result;
}

} // namespace nx::sql
