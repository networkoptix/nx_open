// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_queue.h"

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/time.h>

#include "multiple_query_executor.h"

namespace nx::sql::detail {

void QueryQueue::push(value_type value)
{
    std::size_t queueSize = 0;

    {
        // Adding query to the m_preliminaryQueue first to minimize the mutex lock time.
        std::lock_guard<std::mutex> lock(m_preliminaryQueueMutex);

        m_preliminaryQueue.push_back({
            .value = std::move(value),
            .enqueueTime = nx::utils::monotonicTime()});
        ++m_preliminaryQueueSize;

        queueSize = m_preliminaryQueue.size();
    }

    m_cond.wakeAll();

    NX_TRACE(this, "QueryQueue::push done. Preliminary queue size %1", queueSize);
}

QueryQueueStats QueryQueue::stats() const
{
    using namespace std::chrono;

    std::size_t queueSize = 0;
    {
        std::lock_guard<std::mutex> lock(m_preliminaryQueueMutex);
        queueSize = m_preliminaryQueue.size();
    }

    NX_MUTEX_LOCKER lock(&m_mainQueueMutex);

    milliseconds oldestQueryAge = milliseconds::zero();

    const auto now = steady_clock::now();
    for (const auto& [p, q]: m_priorityToQueue)
    {
        if (!q.empty())
            oldestQueryAge = std::max(oldestQueryAge, floor<milliseconds>(now - q.front().enqueueTime));
        queueSize += q.size();
    }

    return QueryQueueStats{
        .pendingQueryCount = (int) queueSize,
        .oldestQueryAge = oldestQueryAge};
}

std::size_t QueryQueue::pendingQueryCount() const
{
    return m_preliminaryQueueSize.load() + m_pendingQueryCount.load();
}

std::optional<QueryQueue::value_type> QueryQueue::pop(
    std::optional<std::chrono::milliseconds> timeout)
{
    WaitConditionTimer waitTimer(
        &m_cond,
        timeout ? *timeout : std::chrono::milliseconds::max());

    QuerySelectionContext querySelectionContext;

    std::vector<QueryQueue::value_type> resultingQueries;
    for (;;)
    {
        moveFromPreliminaryToMainQueue();

        NX_MUTEX_LOCKER lock(&m_mainQueueMutex);

        removeExpiredElements(&lock);

        std::optional<FoundQueryContext> queryQueueElementContext =
            getNextSuitableQuery(&querySelectionContext, resultingQueries.empty());
        if (queryQueueElementContext)
        {
            if (canAggregate(resultingQueries, queryQueueElementContext->value))
            {
                resultingQueries.push_back(std::move(queryQueueElementContext->value));
                pop(*queryQueueElementContext);
                continue;
            }
            else
            {
                // Aggregation with empty resultingQueries is always possible.
                NX_ASSERT(!resultingQueries.empty());
                return aggregateQueries(std::exchange(resultingQueries, {}));
            }
        }

        if (!resultingQueries.empty())
            return aggregateQueries(std::exchange(resultingQueries, {}));

        querySelectionContext = QuerySelectionContext();

        if (!waitTimer.wait(lock.mutex()))
            return std::nullopt;
    }
}

void QueryQueue::setItemStayTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    NX_MUTEX_LOCKER lock(&m_mainQueueMutex);
    m_itemStayTimeout = timeout;
}

void QueryQueue::setOnItemStayTimeout(ItemStayTimeoutHandler itemStayTimeoutHandler)
{
    m_itemStayTimeoutHandler = std::move(itemStayTimeoutHandler);
}

void QueryQueue::reportTimedOutQueries()
{
    moveFromPreliminaryToMainQueue();

    NX_MUTEX_LOCKER lock(&m_mainQueueMutex);
    removeExpiredElements(&lock);
}

void QueryQueue::setConcurrentModificationQueryLimit(int value)
{
    m_concurrentModificationQueryLimit = value;
}

int QueryQueue::concurrentModificationCount() const
{
    return m_currentModificationCount;
}

void QueryQueue::setQueryPriority(QueryType queryType, int newPriority)
{
    NX_MUTEX_LOCKER lock(&m_mainQueueMutex);
    m_customPriorities.emplace(queryType, newPriority);
}

void QueryQueue::setAggregationLimit(int limit)
{
    m_aggregationLimit = limit;
}

int QueryQueue::aggregationLimit() const
{
    return m_aggregationLimit;
}

void QueryQueue::moveFromPreliminaryToMainQueue()
{
    Queries lightQueue;
    {
        std::lock_guard<std::mutex> lock(m_preliminaryQueueMutex);
        std::swap(lightQueue, m_preliminaryQueue);
    }

    NX_MUTEX_LOCKER lock(&m_mainQueueMutex);

    // Enqueuing tasks.
    for (auto& task: lightQueue)
    {
        const auto priority = getPriority(*task.value);
        m_priorityToQueue[priority].push_back(std::move(task));
        --m_preliminaryQueueSize;
        ++m_pendingQueryCount;
    }
}

int QueryQueue::getPriority(const AbstractExecutor& value) const
{
    const auto priorityIter = m_customPriorities.find(value.queryType());
    return priorityIter == m_customPriorities.end()
        ? 0
        : priorityIter->second;
}

std::optional<QueryQueue::FoundQueryContext> QueryQueue::getNextSuitableQuery(
    QuerySelectionContext* querySelectionContext,
    bool consumeLimits)
{
    for (auto& [priority, queue]: m_priorityToQueue)
        for (auto it = queue.begin(); it != queue.end(); ++it)
        {
            auto& query = it->value;

            if (nx::utils::contains(querySelectionContext->forbiddenQueryTypes, query->queryType()))
                continue;

            if (!consumeLimits || checkAndUpdateQueryLimits(query))
                return FoundQueryContext{.value = query, .priority = priority, .it = it};

            if (consumeLimits)
            {
                // Limits check didn't pass. Making sure we are not selecting a query of the same type
                // on this iteration since it will lead to query reordering.
                querySelectionContext->forbiddenQueryTypes.push_back(query->queryType());
            }
        }

    return std::nullopt;
}

void QueryQueue::pop(const FoundQueryContext& queryContext)
{
    m_priorityToQueue[queryContext.priority].erase(queryContext.it);
    --m_pendingQueryCount;
}

bool QueryQueue::checkAndUpdateQueryLimits(
    const std::unique_ptr<AbstractExecutor>& nextQuery)
{
    if (nextQuery->queryType() == QueryType::lookup)
        return true;

    if (m_concurrentModificationQueryLimit == 0)
        return true;

    for (;;)
    {
        int currentModificationCount = m_currentModificationCount.load();
        const int newModificationCount = currentModificationCount+1;
        if (!m_currentModificationCount.compare_exchange_strong(
                currentModificationCount,
                newModificationCount))
        {
            if (currentModificationCount > m_concurrentModificationQueryLimit)
                return false;
            continue;
        }

        if (newModificationCount <= m_concurrentModificationQueryLimit)
        {
            nextQuery->setOnBeforeDestruction(
                std::bind(&QueryQueue::decreaseLimitCounters, this, nextQuery.get()));
            return true;
        }

        --m_currentModificationCount;
        return false;
    }
}

bool QueryQueue::canAggregate(
    const std::vector<QueryQueue::value_type>& queries,
    const QueryQueue::value_type& query) const
{
    if (queries.empty())
        return true;

    if ((m_aggregationLimit >= 0) && ((int) queries.size() >= m_aggregationLimit))
        return false;

    if (queries.back()->aggregationKey().empty() ||
        query->aggregationKey().empty())
    {
        return false;
    }

    if (queries.back()->queryType() != query->queryType())
        return false;

    return queries.back()->aggregationKey() == query->aggregationKey();
}

void QueryQueue::decreaseLimitCounters(AbstractExecutor* finishedQuery)
{
    if (m_concurrentModificationQueryLimit > 0 &&
        finishedQuery->queryType() == QueryType::modification)
    {
        if (--m_currentModificationCount < m_concurrentModificationQueryLimit)
            m_cond.wakeAll();
    }
}

void QueryQueue::removeExpiredElements(nx::Locker<nx::Mutex>* lock)
{
    if (!m_itemStayTimeout || !m_itemStayTimeoutHandler)
        return;

    const auto minEnqueueTime = nx::utils::monotonicTime() - *m_itemStayTimeout;
    for (auto& [priority, queue]: m_priorityToQueue)
    {
        while (!queue.empty() && queue.front().enqueueTime <= minEnqueueTime)
        {
            auto value = std::exchange(queue.front().value, {});
            queue.pop_front();
            --m_pendingQueryCount;

            nx::Unlocker<nx::Mutex> unlocker(lock);
            m_itemStayTimeoutHandler(std::move(value));
        }
    }
}

QueryQueue::value_type QueryQueue::aggregateQueries(
    std::vector<QueryQueue::value_type> queries)
{
    if (queries.size() == 1U)
        return std::move(queries.front());

    return std::make_unique<MultipleQueryExecutor>(std::move(queries));
}

} // namespace nx::sql::detail
