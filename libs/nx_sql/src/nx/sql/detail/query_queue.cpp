// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_queue.h"

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/time.h>

#include "multiple_query_executor.h"

namespace nx::sql::detail {

const int QueryQueue::kDefaultPriority;

QueryQueue::QueryQueue():
    m_currentModificationCount(0)
{
}

void QueryQueue::push(value_type value)
{
    nx::utils::ElapsedTimer t;
    t.restart();

    std::size_t queueSize = 0;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        const auto priority = getPriority(*value);
        auto elementIter = m_elementsByPriority.emplace(
            priority, ElementContext{std::move(value), std::nullopt});

        queueSize = m_elementsByPriority.size();

        if (m_itemStayTimeout)
            addElementExpirationTimer(elementIter);
    }

    m_cond.wakeAll();

    NX_TRACE(this, "QueryQueue::push done in %1, queue size %2", t.elapsed(), queueSize);
}

std::size_t QueryQueue::size() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_elementsByPriority.size();
}

std::optional<QueryQueue::value_type> QueryQueue::pop(
    std::optional<std::chrono::milliseconds> timeout)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    WaitConditionTimer waitTimer(
        &m_cond,
        timeout ? *timeout : std::chrono::milliseconds::max());

    QuerySelectionContext querySelectionContext;

    std::vector<QueryQueue::value_type> resultingQueries;
    for (;;)
    {
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

void QueryQueue::reportTimedOutQueries()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    removeExpiredElements(&lock);
}

void QueryQueue::enableItemStayTimeoutEvent(
    std::chrono::milliseconds timeout,
    ItemStayTimeoutHandler itemStayTimeoutHandler)
{
    m_itemStayTimeout = timeout;
    m_itemStayTimeoutHandler = std::move(itemStayTimeoutHandler);
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
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    for (auto it = m_elementsByPriority.begin(); it != m_elementsByPriority.end(); ++it)
    {
        auto& query = it->second.value;

        if (nx::utils::contains(querySelectionContext->forbiddenQueryTypes, query->queryType()))
            continue;

        if (!consumeLimits || checkAndUpdateQueryLimits(query))
            return FoundQueryContext{query, it};

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
    removeExpirationTimer(queryContext.it->second);
    m_elementsByPriority.erase(queryContext.it);
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

void QueryQueue::addElementExpirationTimer(
    typename ElementsByPriority::iterator elementIter)
{
    auto timerIter = m_elementExpirationTimers.emplace(
        nx::utils::monotonicTime() + *m_itemStayTimeout,
        ElementExpirationContext{elementIter});
    elementIter->second.timerIter = timerIter;
}

void QueryQueue::removeExpirationTimer(const ElementContext& elementContext)
{
    if (elementContext.timerIter)
        m_elementExpirationTimers.erase(*elementContext.timerIter);
}

void QueryQueue::removeExpiredElements(nx::Locker<nx::Mutex>* lock)
{
    const auto now = nx::utils::monotonicTime();
    while (!m_elementExpirationTimers.empty() &&
        m_elementExpirationTimers.begin()->first <= now)
    {
        auto expirationContext = std::move(m_elementExpirationTimers.begin()->second);
        m_elementExpirationTimers.erase(m_elementExpirationTimers.begin());

        value_type value;
        if (expirationContext.elementsByPriorityIter)
        {
            value = std::move((*expirationContext.elementsByPriorityIter)->second.value);
            m_elementsByPriority.erase(*expirationContext.elementsByPriorityIter);
        }

        nx::Unlocker<nx::Mutex> unlocker(lock);
        m_itemStayTimeoutHandler(std::move(value));
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
