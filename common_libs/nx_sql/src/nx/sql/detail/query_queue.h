#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <map>
#include <list>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include "../request_executor.h"

namespace nx {
namespace sql {
namespace detail {

class NX_SQL_API QueryQueue
{
public:
    using value_type = std::unique_ptr<AbstractExecutor>;
    using ItemStayTimeoutHandler = nx::utils::MoveOnlyFunc<void(value_type)>;

    static const int kDefaultPriority = 0;

    QueryQueue();

    void enableItemStayTimeoutEvent(
        std::chrono::milliseconds timeout,
        ItemStayTimeoutHandler itemStayTimeoutHandler);

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);

    /**
     * By default, every query has priority of 0.
     */
    void setQueryPriority(QueryType queryType, int newPriority);

    void push(value_type query);

    std::size_t size() const;

    std::optional<value_type> pop(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

private:
    struct ElementContext;

    using ElementsByPriority = std::multimap<int, ElementContext, std::greater<int>>;
    using PostponedElements = std::list<ElementContext>;

    /**
     * Element can be in either m_elementsByPriority or m_postponedElements.
     */
    struct ElementExpirationContext
    {
        std::optional<typename ElementsByPriority::iterator> elementsByPriorityIter;
        std::optional<typename PostponedElements::iterator> postponedElementsIter;
    };

    using ElementExpirationTimers =
        std::multimap<std::chrono::steady_clock::time_point, ElementExpirationContext>;

    struct ElementContext
    {
        value_type value;
        std::optional<typename ElementExpirationTimers::iterator> timerIter;
    };

    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;

    std::map<QueryType, int> m_customPriorities;
    std::atomic<int> m_currentModificationCount;
    int m_concurrentModificationQueryLimit = 0;
    ElementsByPriority m_elementsByPriority;
    PostponedElements m_postponedElements;
    ElementExpirationTimers m_elementExpirationTimers;

    std::optional<std::chrono::milliseconds> m_itemStayTimeout;
    ItemStayTimeoutHandler m_itemStayTimeoutHandler;

    int getPriority(const AbstractExecutor& query) const;

    std::optional<QueryQueue::value_type> popPostponedElement();

    void decreaseLimitCounters(AbstractExecutor* finishedQuery);

    bool checkAndUpdateQueryLimits(
        const std::unique_ptr<AbstractExecutor>& queryExecutor);

    void addElementExpirationTimer(
        typename ElementsByPriority::iterator elementIter);

    void removeExpirationTimer(const ElementContext& elementContext);

    void postponeElement(ElementContext elementContext);

    void removeExpiredElements(QnMutexLockerBase* lock);
};

} // namespace detail
} // namespace sql
} // namespace nx
