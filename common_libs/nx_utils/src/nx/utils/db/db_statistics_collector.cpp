#include "db_statistics_collector.h"

#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace db {

QueryExecutionInfo::QueryExecutionInfo():
    waitForExecutionDuration(std::chrono::milliseconds::zero())
{
}

DurationStatistics::DurationStatistics():
    min(std::chrono::milliseconds::max()),
    max(std::chrono::milliseconds::min()),
    average(std::chrono::milliseconds::zero())
{
}

QueryStatistics::QueryStatistics():
    requestsSucceeded(0),
    requestsFailed(0),
    requestsCancelled(0)
{
}

StatisticsCollector::DurationStatisticsCalculationContext::DurationStatisticsCalculationContext(
    DurationStatistics* result)
    :
    result(result),
    currentSum(std::chrono::milliseconds::zero()),
    count(0),
    recalcMinMax(false)
{
}

StatisticsCollector::StatisticsRecordContext::StatisticsRecordContext(QueryExecutionInfo data):
    data(std::move(data)),
    timestamp(nx::utils::monotonicTime())
{
}

StatisticsCollector::StatisticsCollector(
    std::chrono::milliseconds period)
    :
    m_period(period),
    m_requestExecutionTimesCalculationContext(&m_currentStatistics.requestExecutionTimes),
    m_waitingForExecutionTimesCalculationContext(&m_currentStatistics.waitingForExecutionTimes)
{
    m_currentStatistics.statisticalPeriod = m_period;
}

void StatisticsCollector::recordQuery(QueryExecutionInfo queryStatistics)
{
    QnMutexLocker lock(&m_mutex);

    updateStatisticsWithNewValue(queryStatistics);
    m_recordQueue.push_back(StatisticsRecordContext(queryStatistics));
}

QueryStatistics StatisticsCollector::getQueryStatistics() const
{
    QnMutexLocker lock(&m_mutex);
    const_cast<StatisticsCollector*>(this)->removeExpiredRecords(lock);
    return m_currentStatistics;
}

std::chrono::milliseconds StatisticsCollector::aggregationPeriod() const
{
    return m_period;
}

void StatisticsCollector::clearStatistics()
{
    m_recordQueue.clear();
    m_currentStatistics = QueryStatistics();
    m_currentStatistics.statisticalPeriod = m_period;
    m_requestExecutionTimesCalculationContext =
        DurationStatisticsCalculationContext(&m_currentStatistics.requestExecutionTimes);
    m_waitingForExecutionTimesCalculationContext =
        DurationStatisticsCalculationContext(&m_currentStatistics.waitingForExecutionTimes);
}

void StatisticsCollector::updateStatisticsWithNewValue(
    const QueryExecutionInfo& queryStatistics)
{
    if (queryStatistics.result)
    {
        if (*queryStatistics.result == DBResult::ok)
            ++m_currentStatistics.requestsSucceeded;
        else
            ++m_currentStatistics.requestsFailed;
    }
    else
    {
        ++m_currentStatistics.requestsCancelled;
    }

    addValue(
        &m_waitingForExecutionTimesCalculationContext,
        queryStatistics.waitForExecutionDuration);

    if (queryStatistics.executionDuration)
    {
        addValue(
            &m_requestExecutionTimesCalculationContext,
            *queryStatistics.executionDuration);
    }
}

void StatisticsCollector::addValue(
    DurationStatisticsCalculationContext* calculationContext,
    std::chrono::milliseconds value)
{
    calculationContext->currentSum += value;
    updateMinMax(calculationContext->result, value);
    ++calculationContext->count;
    calculationContext->result->average =
        calculationContext->currentSum / calculationContext->count;
}

void StatisticsCollector::removeExpiredRecords(const QnMutexLockerBase& /*lock*/)
{
    if (m_recordQueue.empty())
        return;

    const auto t = nx::utils::monotonicTime() - m_period;
    auto posToKeep = m_recordQueue.begin();
    for (; posToKeep != m_recordQueue.end(); ++posToKeep)
    {
        if (posToKeep->timestamp >= t)
            break;

        removeValueFromStatistics(posToKeep->data);
    }

    m_recordQueue.erase(m_recordQueue.begin(), posToKeep);

    recalcIfNeeded();
}

void StatisticsCollector::removeValueFromStatistics(const QueryExecutionInfo& queryStatistics)
{
    if (queryStatistics.result)
    {
        if (*queryStatistics.result == DBResult::ok)
            --m_currentStatistics.requestsSucceeded;
        else
            --m_currentStatistics.requestsFailed;
    }
    else
    {
        --m_currentStatistics.requestsCancelled;
    }

    removeValue(
        &m_waitingForExecutionTimesCalculationContext,
        queryStatistics.waitForExecutionDuration);

    if (queryStatistics.executionDuration)
    {
        removeValue(
            &m_requestExecutionTimesCalculationContext,
            *queryStatistics.executionDuration);
    }
}

void StatisticsCollector::removeValue(
    DurationStatisticsCalculationContext* calculationContext,
    std::chrono::milliseconds value)
{
    using namespace std::chrono;

    calculationContext->currentSum -= value;
    if (calculationContext->result->min == value || calculationContext->result->max == value)
        calculationContext->recalcMinMax = true;
    // TODO: #ak apply min_max_queue here.

    NX_ASSERT(calculationContext->count > 0);
    --calculationContext->count;
    calculationContext->result->average =
        calculationContext->count > 0
        ? duration_cast<milliseconds>(calculationContext->currentSum / calculationContext->count)
        : milliseconds::zero();
}

void StatisticsCollector::recalcIfNeeded()
{
    if (m_requestExecutionTimesCalculationContext.recalcMinMax ||
        m_waitingForExecutionTimesCalculationContext.recalcMinMax)
    {
        m_requestExecutionTimesCalculationContext.result->min = std::chrono::milliseconds::max();
        m_requestExecutionTimesCalculationContext.result->max = std::chrono::milliseconds::min();

        m_waitingForExecutionTimesCalculationContext.result->min = std::chrono::milliseconds::max();
        m_waitingForExecutionTimesCalculationContext.result->max = std::chrono::milliseconds::min();

        for (const auto& record: m_recordQueue)
        {
            if (record.data.executionDuration)
            {
                updateMinMax(
                    m_requestExecutionTimesCalculationContext.result,
                    *record.data.executionDuration);
            }

            updateMinMax(
                m_waitingForExecutionTimesCalculationContext.result,
                record.data.waitForExecutionDuration);
        }

        m_requestExecutionTimesCalculationContext.recalcMinMax = false;
        m_waitingForExecutionTimesCalculationContext.recalcMinMax = false;
    }
}

void StatisticsCollector::updateMinMax(
    DurationStatistics* result,
    std::chrono::milliseconds value)
{
    result->min = std::min(result->min, value);
    result->max = std::max(result->max, value);
}

} // namespace db
} // namespace utils
} // namespace nx
