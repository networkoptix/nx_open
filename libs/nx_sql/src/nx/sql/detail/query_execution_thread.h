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
    using base_type = BaseQueryExecutor;

public:
    QueryExecutionThread(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* queryExecutorQueue);

    QueryExecutionThread(
        const ConnectionOptions& connectionOptions,
        std::unique_ptr<AbstractDbConnection> connection,
        QueryExecutorQueue* queryExecutorQueue);

    virtual ~QueryExecutionThread() override;

    virtual void pleaseStop() override;
    virtual void join() override;

    virtual ConnectionState state() const override;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void start(std::chrono::milliseconds connectDelay = std::chrono::milliseconds::zero()) override;

private:
    void handleExecutionResult(DBResult result, DbConnectionHolder* dbConnectionHolder);

    void queryExecutionThreadMain();
    void closeConnection(DbConnectionHolder* dbConnectionHolder);

    static bool isDbErrorRecoverable(DBResult dbResult);

private:
    std::unique_ptr<AbstractDbConnection> m_externalConnection;
    std::chrono::milliseconds m_connectDelay = std::chrono::milliseconds::zero();
    std::atomic<ConnectionState> m_state{ConnectionState::initializing};
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;
    std::thread m_queryExecutionThread;
    std::atomic<bool> m_terminated{false};
    int m_numberOfFailedRequestsInARow = 0;
};

} // namespace nx::sql::detail
