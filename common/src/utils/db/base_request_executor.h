#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/stoppable.h>

#include "request_executor.h"

namespace nx {
namespace db {

enum class ConnectionState
{
    initializing,
    opened,
    closed
};

typedef nx::utils::SyncQueueWithItemStayTimeout<
    std::unique_ptr<AbstractExecutor>
> QueryExecutorQueue;

class BaseRequestExecutor:
    public QnStoppable,
    public QnJoinable
{
public:
    BaseRequestExecutor(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
    virtual ~BaseRequestExecutor() = default;

    virtual ConnectionState state() const = 0;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    virtual void start() = 0;

    const ConnectionOptions& connectionOptions() const;

protected:
    QueryExecutorQueue* queryExecutorQueue();
    const QueryExecutorQueue* queryExecutorQueue() const;

private:
    ConnectionOptions m_connectionOptions;
    QueryExecutorQueue* const m_queryExecutorQueue;
};

} // namespace db
} // namespace nx
