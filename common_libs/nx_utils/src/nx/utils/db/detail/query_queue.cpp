#include "query_queue.h"

#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace db {
namespace detail {

const int QueryQueue::kDefaultPriority;

QueryQueue::QueryQueue():
    m_currentModificationCount(0)
{
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

void QueryQueue::setQueryPriority(QueryType queryType, int newPriority)
{
    QnMutexLocker lock(&m_mutex);
    m_customPriorities.emplace(queryType, newPriority);
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

    for (;;)
    {
        removeExpiredElements(&lock);

        auto value = popPostponedElement();
        if (value)
            return value;

        if (!m_elementsByPriority.empty())
        {
            auto elementContext = std::move(m_elementsByPriority.begin()->second);
            m_elementsByPriority.erase(m_elementsByPriority.begin());

            if (checkAndUpdateQueryLimits(elementContext.value))
            {
                removeExpirationTimer(elementContext);
                return std::move(elementContext.value);
            }

            postponeElement(std::move(elementContext));
            continue;
        }

        if (!waitTimer.wait(lock.mutex()))
            return std::nullopt;
    }
}

int QueryQueue::getPriority(const AbstractExecutor& value) const
{
    const auto priorityIter = m_customPriorities.find(value.queryType());
    return priorityIter == m_customPriorities.end()
        ? 0
        : priorityIter->second;
}

std::optional<QueryQueue::value_type> QueryQueue::popPostponedElement()
{
    if (!m_postponedElements.empty() &&
        checkAndUpdateQueryLimits(m_postponedElements.front().value))
    {
        removeExpirationTimer(m_postponedElements.front());
        auto value = std::move(m_postponedElements.front().value);
        m_postponedElements.pop_front();
        return std::move(value);
    }

    return std::nullopt;
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

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
