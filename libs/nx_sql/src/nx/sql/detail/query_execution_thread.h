// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <thread>

#include <nx/utils/std/thread.h>
#include <nx/utils/thread/long_runnable.h>

#include "base_query_executor.h"
#include "query_executor.h"
#include "../db_connection_holder.h"

namespace nx::sql::detail {

/**
 * Connection can be closed by timeout or due to error.
 * Use QueryExecutionThread::isOpen to test it.
 */
class NX_SQL_API QueryExecutionThread:
    public BaseQueryExecutor
{
public:
    QueryExecutionThread(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);

    QueryExecutionThread(
        const ConnectionOptions& connectionOptions,
        std::unique_ptr<AbstractDbConnection> connection,
        QueryExecutorQueue* const queryExecutorQueue);

    virtual ~QueryExecutionThread() override;

    virtual void pleaseStop() override;
    virtual void join() override;

    virtual ConnectionState state() const override;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void start(std::chrono::milliseconds connectDelay = std::chrono::milliseconds::zero()) override;

protected:
    virtual void processTask(std::unique_ptr<AbstractExecutor> task);

    AbstractDbConnection* connection();
    void handleExecutionResult(DBResult result);

private:
    std::chrono::milliseconds m_connectDelay = std::chrono::milliseconds::zero();
    std::atomic<ConnectionState> m_state{ConnectionState::initializing};
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;
    std::thread m_queryExecutionThread;
    std::atomic<bool> m_terminated{false};
    int m_numberOfFailedRequestsInARow = 0;
    DbConnectionHolder m_dbConnectionHolder;

    void queryExecutionThreadMain();
    void closeConnection();

    static bool isDbErrorRecoverable(DBResult dbResult);
};

} // namespace nx::sql::detail
