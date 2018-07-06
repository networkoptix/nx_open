#include "query_queue.h"

namespace nx {
namespace utils {
namespace db {
namespace detail {

QueryQueue::QueryQueue():
    m_currentModificationCount(0)
{
}

void QueryQueue::setConcurrentModificationQueryLimit(int value)
{
    m_concurrentModificationQueryLimit = value;
}

boost::optional<std::unique_ptr<AbstractExecutor>> QueryQueue::pop(
    boost::optional<std::chrono::milliseconds> timeout,
    QueueReaderId readerId)
{
    using namespace std::placeholders;

    return base_type::popIf(
        std::bind(&QueryQueue::checkAndUpdateQueryLimits, this, _1),
        timeout,
        readerId);
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
            base_type::retestPopIfCondition();
    }
}

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
