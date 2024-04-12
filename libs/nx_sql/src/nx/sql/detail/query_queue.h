// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <map>
#include <list>
#include <limits>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include "query_executor.h"

namespace nx::sql::detail {

class NX_SQL_API QueryQueue
{
public:
    using value_type = std::unique_ptr<AbstractExecutor>;
    using ItemStayTimeoutHandler = nx::utils::MoveOnlyFunc<void(value_type)>;

    static const int kDefaultPriority = 0;

    QueryQueue();

    void push(value_type query);

    std::size_t size() const;

    std::optional<value_type> pop(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * Checks for timed out queries and calls the timeout handler for each of them within this
     * fuction. Does not block.
     */
    void reportTimedOutQueries();

    void enableItemStayTimeoutEvent(
        std::chrono::milliseconds timeout,
        ItemStayTimeoutHandler itemStayTimeoutHandler);

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);
    int concurrentModificationCount() const;

    /**
     * By default, every query has priority of 0.
     */
    void setQueryPriority(QueryType queryType, int newPriority);

    /**
     * @param limit If less than zero, than no limit is applied.
     * By default, there is no limit.
     */
    void setAggregationLimit(int limit);
    int aggregationLimit() const;

private:
    struct ElementContext;

    using ElementsByPriority = std::multimap<int, ElementContext, std::greater<int>>;

    /**
     * Element can be in either m_elementsByPriority or m_postponedElements.
     */
    struct ElementExpirationContext
    {
        std::optional<typename ElementsByPriority::iterator> elementsByPriorityIter;
    };

    using ElementExpirationTimers =
        std::multimap<std::chrono::steady_clock::time_point, ElementExpirationContext>;

    struct ElementContext
    {
        value_type value;
        std::optional<typename ElementExpirationTimers::iterator> timerIter;
    };

    struct FoundQueryContext
    {
        value_type& value;
        ElementsByPriority::iterator it;
    };

    struct QuerySelectionContext
    {
        std::vector<QueryType> forbiddenQueryTypes;
    };

    mutable nx::Mutex m_mutex;
    nx::WaitCondition m_cond;

    std::map<QueryType, int> m_customPriorities;
    std::atomic<int> m_currentModificationCount;
    int m_concurrentModificationQueryLimit = 0;
    int m_aggregationLimit = -1;
    ElementsByPriority m_elementsByPriority;
    ElementExpirationTimers m_elementExpirationTimers;

    std::optional<std::chrono::milliseconds> m_itemStayTimeout;
    ItemStayTimeoutHandler m_itemStayTimeoutHandler;

    int getPriority(const AbstractExecutor& query) const;

    std::optional<FoundQueryContext> getNextSuitableQuery(
        QuerySelectionContext* querySelectionContext,
        bool consumeLimits);

    void pop(const FoundQueryContext&);

    bool canAggregate(
        const std::vector<QueryQueue::value_type>& queries,
        const QueryQueue::value_type& query) const;

    void decreaseLimitCounters(AbstractExecutor* finishedQuery);

    bool checkAndUpdateQueryLimits(
        const std::unique_ptr<AbstractExecutor>& queryExecutor);

    void addElementExpirationTimer(
        typename ElementsByPriority::iterator elementIter);

    void removeExpirationTimer(const ElementContext& elementContext);

    void removeExpiredElements(nx::Locker<nx::Mutex>* lock);

    QueryQueue::value_type aggregateQueries(std::vector<QueryQueue::value_type> queries);
};

} // namespace nx::sql::detail
