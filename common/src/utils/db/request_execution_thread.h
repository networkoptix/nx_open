#pragma once

#include <atomic>
#include <memory>

#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>
#include <nx/utils/move_only_func.h>

#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>

#include "request_executor.h"

namespace nx {
namespace db {

enum class ConnectionState
{
    initializing,
    opened,
    closed
};

/**
 * Connection can be closed by timeout or due to error. 
 * Use DbRequestExecutionThread::isOpen to test it.
 */
class DbRequestExecutionThread
:
    public QnLongRunnable
{
public:
    typedef nx::utils::SyncQueueWithItemStayTimeout<
        std::unique_ptr<AbstractExecutor>
    > QueryExecutorQueue;

    DbRequestExecutionThread(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
    virtual ~DbRequestExecutionThread();

    /**
     * Establishes connection to DB.
     * This method MUST be called after class instanciation
     * @note Method is needed because we do not use exceptions
     */
    bool open();

    ConnectionState state() const;

    void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler);

protected:
    /** Implementation of QnLongRunnable::run. */
    virtual void run() override;

private:
    ConnectionOptions m_connectionOptions;
    QSqlDatabase m_dbConnection;
    QueryExecutorQueue* const m_queryExecutorQueue;
    ConnectionState m_state;
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;

    bool tuneConnection();
    bool tuneMySqlConnection();

    static bool isDbErrorRecoverable(DBResult dbResult);
};

} // namespace db
} // namespace nx
