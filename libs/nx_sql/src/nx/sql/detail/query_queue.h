// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>

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

    static constexpr int kDefaultPriority = 0;

    void push(value_type query);

    QueryQueueStatistics stats() const;

    /**
     * Wait-free version of stats().pendingQueryCount.
     */
    std::size_t pendingQueryCount() const;

    std::optional<value_type> pop(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    void setItemStayTimeout(std::optional<std::chrono::milliseconds> timeout);

    void setOnItemStayTimeout(ItemStayTimeoutHandler itemStayTimeoutHandler);

    /**
     * Checks for timed out queries and calls the timeout handler for each of them within this
     * fuction. Does not block.
     */
    void reportTimedOutQueries();

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);
    int concurrentModificationCount() const;

    /**
     * Queries with bigger priority are executed first.
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
    struct ElementContext
    {
        value_type value;
        std::chrono::steady_clock::time_point enqueueTime;
    };

    using Queries = std::deque<ElementContext>;

    struct FoundQueryContext
    {
        value_type& value;
        int priority = 0;
        Queries::iterator it;
    };

    struct QuerySelectionContext
    {
        std::vector<QueryType> forbiddenQueryTypes;
    };

    mutable nx::Mutex m_mainQueueMutex;
    // NOTE: It appears that using std::mutex here leads to 2-3x better throughput of tasks
    // through this class in a multithreaded environment.
    mutable std::mutex m_preliminaryQueueMutex;
    Queries m_preliminaryQueue;
    nx::WaitCondition m_cond;

    std::map<QueryType, int> m_customPriorities;
    std::atomic<int> m_currentModificationCount{0};
    int m_concurrentModificationQueryLimit = 0;
    int m_aggregationLimit = -1;
    std::map<int, Queries, std::greater<int>> m_priorityToQueue;
    std::atomic<std::size_t> m_preliminaryQueueSize{0};
    std::atomic<std::size_t> m_pendingQueryCount{0};

    std::optional<std::chrono::milliseconds> m_itemStayTimeout;
    ItemStayTimeoutHandler m_itemStayTimeoutHandler;

    void moveFromPreliminaryToMainQueue();

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

    void removeExpiredElements(nx::Locker<nx::Mutex>* lock);

    QueryQueue::value_type aggregateQueries(std::vector<QueryQueue::value_type> queries);
};

} // namespace nx::sql::detail
