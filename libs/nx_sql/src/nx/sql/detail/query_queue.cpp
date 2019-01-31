#include "query_queue.h"

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
    {
        QnMutexLocker lock(&m_mutex);

        const auto priority = getPriority(*value);
        auto elementIter = m_elementsByPriority.emplace(
            priority, ElementContext{std::move(value), std::nullopt});

        if (m_itemStayTimeout)
            addElementExpirationTimer(elementIter);
    }

    m_cond.wakeAll();
}

std::size_t QueryQueue::size() const
{
    QnMutexLocker lock(&m_mutex);

    return m_elementsByPriority.size() + m_postponedElements.size();
}

std::optional<QueryQueue::value_type> QueryQueue::pop(
    std::optional<std::chrono::milliseconds> timeout)
{
    QnMutexLocker lock(&m_mutex);

    WaitConditionTimer waitTimer(
        &m_cond,
        timeout ? *timeout : std::chrono::milliseconds::max());

    std::vector<QueryQueue::value_type> resultingQueries;
    for (;;)
    {
        removeExpiredElements(&lock);

        std::optional<FoundQueryContext> queryQueueElementContext =
            postponeUntilNextSuitableQuery(resultingQueries.empty());
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

        if (!waitTimer.wait(lock.mutex()))
            return std::nullopt;
    }
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
    QnMutexLocker lock(&m_mutex);
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

std::optional<QueryQueue::FoundQueryContext>
    QueryQueue::postponeUntilNextSuitableQuery(bool consumeLimits)
{
    for (;;)
    {
        if (!m_postponedElements.empty())
        {
            auto& query = m_postponedElements.front().value;

            if (!consumeLimits || checkAndUpdateQueryLimits(query))
                return FoundQueryContext{query, QueueType::postponedQueries};
        }

        if (!m_elementsByPriority.empty())
        {
            auto& query = m_elementsByPriority.begin()->second.value;

            if (!consumeLimits || checkAndUpdateQueryLimits(query))
                return FoundQueryContext{query, QueueType::queriesByPriority};

            if (consumeLimits)
            {
                // checkAndUpdateQueryLimits failed.
                postponeTopQuery();
                continue;
            }
        }

        return std::nullopt;
    }
}

void QueryQueue::pop(const FoundQueryContext& queryContext)
{
    if (queryContext.queueType == QueueType::postponedQueries)
    {
        removeExpirationTimer(m_postponedElements.front());
        m_postponedElements.pop_front();
    }
    else if (queryContext.queueType == QueueType::queriesByPriority)
    {
        removeExpirationTimer(m_elementsByPriority.begin()->second);
        m_elementsByPriority.erase(m_elementsByPriority.begin());
    }
}

void QueryQueue::postponeTopQuery()
{
    auto elementContext = std::move(m_elementsByPriority.begin()->second);
    m_elementsByPriority.erase(m_elementsByPriority.begin());
    postponeElement(std::move(elementContext));
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
        ElementExpirationContext{elementIter, std::nullopt});
    elementIter->second.timerIter = timerIter;
}

void QueryQueue::removeExpirationTimer(const ElementContext& elementContext)
{
    if (elementContext.timerIter)
        m_elementExpirationTimers.erase(*elementContext.timerIter);
}

void QueryQueue::postponeElement(ElementContext elementContext)
{
    m_postponedElements.push_back(std::move(elementContext));

    if (m_postponedElements.back().timerIter)
    {
        (*m_postponedElements.back().timerIter)->second.elementsByPriorityIter = std::nullopt;
        (*m_postponedElements.back().timerIter)->second.postponedElementsIter =
            std::prev(m_postponedElements.end());
    }
}

void QueryQueue::removeExpiredElements(QnMutexLockerBase* lock)
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

        if (expirationContext.postponedElementsIter)
        {
            value = std::move((*expirationContext.postponedElementsIter)->value);
            m_postponedElements.erase(*expirationContext.postponedElementsIter);
        }

        QnMutexUnlocker unlocker(lock);
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
