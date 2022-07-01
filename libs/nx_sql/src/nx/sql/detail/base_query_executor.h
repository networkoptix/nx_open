// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/stoppable.h>

#include "query_queue.h"
#include "query_executor.h"

namespace nx::sql::detail {

enum class ConnectionState
{
    initializing,
    opened,
    closed
};

using QueryExecutorQueue = detail::QueryQueue;

class NX_SQL_API BaseQueryExecutor:
    public QnStoppable,
    public QnJoinable
{
public:
    BaseQueryExecutor(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
    virtual ~BaseQueryExecutor() = default;

    virtual ConnectionState state() const = 0;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) = 0;

    /**
     * @param connectDelay Delay before connection to the DB.
     */
    virtual void start(std::chrono::milliseconds connectDelay = std::chrono::milliseconds::zero()) = 0;

    const ConnectionOptions& connectionOptions() const;

protected:
    QueryExecutorQueue* queryExecutorQueue();
    const QueryExecutorQueue* queryExecutorQueue() const;

private:
    ConnectionOptions m_connectionOptions;
    QueryExecutorQueue* const m_queryExecutorQueue;
};

} // namespace nx::sql::detail
