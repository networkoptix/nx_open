#pragma once

#include <atomic>

#include <nx/utils/std/thread.h>

#include <utils/common/long_runnable.h>

#include "base_request_executor.h"
#include "request_executor.h"

namespace nx {
namespace db {

class DbConnectionHolder
{
public:
    DbConnectionHolder(const ConnectionOptions& connectionOptions);
    ~DbConnectionHolder();

    const ConnectionOptions& connectionOptions() const;

    /**
     * Establishes connection to DB.
     * This method MUST be called after class instanciation
     * @note Method is needed because we do not use exceptions
     */
    bool open();

    QSqlDatabase* dbConnection();

    void close();

private:
    QSqlDatabase m_dbConnection;
    const ConnectionOptions m_connectionOptions;

    bool tuneConnection();
    bool tuneMySqlConnection();
};

/**
 * Connection can be closed by timeout or due to error. 
 * Use DbRequestExecutionThread::isOpen to test it.
 */
class DbRequestExecutionThread:
    public BaseRequestExecutor
{
public:
    DbRequestExecutionThread(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
    virtual ~DbRequestExecutionThread();

    virtual void pleaseStop() override;
    virtual void join() override;

    virtual ConnectionState state() const override;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void start() override;

private:
    std::atomic<ConnectionState> m_state;
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;
    nx::utils::thread m_queryExecutionThread;
    std::atomic<bool> m_terminated;
    int m_numberOfFailedRequestsInARow;
    DbConnectionHolder m_dbConnectionHolder;

    void queryExecutionThreadMain();
    void processTask(std::unique_ptr<AbstractExecutor> task);
    void closeConnection();

    static bool isDbErrorRecoverable(DBResult dbResult);
};

} // namespace db
} // namespace nx
