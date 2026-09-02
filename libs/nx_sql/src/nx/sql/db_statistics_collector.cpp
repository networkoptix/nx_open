// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_statistics_collector.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/time.h>

#include "detail/query_queue.h"

namespace nx::sql {

StatisticsCollector::QueryExecutionTaskContext::QueryExecutionTaskContext(
    std::chrono::milliseconds period):
    successfulRequestsCounter(period),
    successfulModificationRequestsCounter(period),
    failedModificationRequestsCounter(period),
    cancelledModificationRequestsCounter(period),
    failedRequestsCounter(period),
    cancelledRequestsCounter(period),
    taskExecutionTimeCounter(period),
    tasksWaitingForExecutionCounter(period)
{}

void StatisticsCollector::QueryExecutionTaskContext::reset()
{
    successfulRequestsCounter.reset();
    successfulModificationRequestsCounter.reset();
    failedModificationRequestsCounter.reset();
    cancelledModificationRequestsCounter.reset();
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

StatisticsCollector::StatisticsCollector(std::chrono::milliseconds period,
    const detail::QueryQueue& queryQueue,
    std::atomic<std::size_t>* dbThreadPoolSize,
    nx::prometheus::Registry* metricsRegistry):
    m_period(period),
    m_queryQueue(queryQueue),
    m_dbThreadPoolSize(dbThreadPoolSize),
    m_metrics(
        metricsRegistry ? std::make_optional(detail::makeMetrics(metricsRegistry)) : std::nullopt),
    m_queryExecutionTaskStatistics(m_period)
{
    if (m_metrics)
    {
        // Publishes this to scrape threads before the constructor returns: read only the members
        // initialised above.
        m_metrics->collector = metricsRegistry->registerCollector(
            [this]()
            {
                const auto queueStatistics = m_queryQueue.stats();
                m_metrics->pendingQueries.set(queueStatistics.pendingQueryCount);
                m_metrics->oldestQueryAge.set(
                    std::chrono::duration<double>(queueStatistics.oldestQueryAge).count());
                m_metrics->threadPoolSize.set(static_cast<double>(m_dbThreadPoolSize->load()));
            });
    }
}

void StatisticsCollector::recordQueryExecutionTask(QueryExecutionTaskRecord record)
{
    const auto outcome = !record.result
        ? detail::Outcome::cancelled
        : (DBResultCode::ok == record.result->code ? detail::Outcome::ok
                                                   : detail::Outcome::failed);

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (record.executionDuration)
            m_queryExecutionTaskStatistics.taskExecutionTimeCounter.add(*record.executionDuration);

        m_queryExecutionTaskStatistics.tasksWaitingForExecutionCounter.add(
            record.waitForExecutionDuration);

        const bool modification = QueryType::modification == record.queryType;
        if (modification)
            ++m_totalModificationRequests;

        if (outcome == detail::Outcome::cancelled)
        {
            m_queryExecutionTaskStatistics.cancelledRequestsCounter.add(1);
            if (modification)
                m_queryExecutionTaskStatistics.cancelledModificationRequestsCounter.add(1);
        }
        else
        {
            const bool succeeded = outcome == detail::Outcome::ok;
            if (modification)
            {
                if (succeeded)
                    m_queryExecutionTaskStatistics.successfulModificationRequestsCounter.add(1);
                else
                    m_queryExecutionTaskStatistics.failedModificationRequestsCounter.add(1);
            }

            if (succeeded)
                m_queryExecutionTaskStatistics.successfulRequestsCounter.add(1);
            else
                m_queryExecutionTaskStatistics.failedRequestsCounter.add(1);
        }
    }

    // Outside m_mutex: prometheus-cpp locks per series, and every DB thread contends on m_mutex.
    if (!m_metrics)
        return;

    const auto queryTypeIndex = static_cast<std::size_t>(record.queryType);
    const auto index = detail::queryTasksIndex(record.queryType, outcome);
    if (!NX_ASSERT(index < m_metrics->queryTasks.size()
                && queryTypeIndex < m_metrics->queryDuration.size()
                && queryTypeIndex < m_metrics->queueWait.size(),
            "Unexpected query type %1",
            static_cast<int>(record.queryType)))
    {
        return;
    }

    m_metrics->queryTasks[index].increment();

    if (record.executionDuration)
        m_metrics->queryDuration[queryTypeIndex].observe(*record.executionDuration);

    m_metrics->queueWait[queryTypeIndex].observe(record.waitForExecutionDuration);
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

    return Statistics{
        .statisticalPeriod = m_period,
        .requestsSucceeded =
            self->m_queryExecutionTaskStatistics.successfulRequestsCounter.getSumPerLastPeriod(),
        .modificationRequestsSucceeded = self->m_queryExecutionTaskStatistics
            .successfulModificationRequestsCounter.getSumPerLastPeriod(),
        .modificationRequestsFailed = self->m_queryExecutionTaskStatistics
            .failedModificationRequestsCounter.getSumPerLastPeriod(),
        .modificationRequestsCancelled = self->m_queryExecutionTaskStatistics
            .cancelledModificationRequestsCounter.getSumPerLastPeriod(),
        .totalModificationRequests = m_totalModificationRequests,
        .requestsFailed =
            self->m_queryExecutionTaskStatistics.failedRequestsCounter.getSumPerLastPeriod(),
        .requestsCancelled =
            self->m_queryExecutionTaskStatistics.cancelledRequestsCounter.getSumPerLastPeriod(),
        .dbThreadPoolSize = m_dbThreadPoolSize->load(),
        .requestExecutionTimes = self->getDurationStatistics(
            &self->m_queryExecutionTaskStatistics.taskExecutionTimeCounter),
        .waitingForExecutionTimes = self->getDurationStatistics(
            &self->m_queryExecutionTaskStatistics.tasksWaitingForExecutionCounter),
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
    m_totalModificationRequests = 0;

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
