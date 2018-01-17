#pragma once

#include <atomic>

#include <nx/utils/std/thread.h>
#include <nx/utils/thread/long_runnable.h>

#include "base_request_executor.h"
#include "db_connection_holder.h"
#include "request_executor.h"

namespace nx {
namespace utils {
namespace db {

/**
 * Connection can be closed by timeout or due to error.
 * Use DbRequestExecutionThread::isOpen to test it.
 */
class NX_UTILS_API DbRequestExecutionThread:
    public BaseRequestExecutor
{
public:
    DbRequestExecutionThread(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue);
    virtual ~DbRequestExecutionThread() override;

    virtual void pleaseStop() override;
    virtual void join() override;

    virtual ConnectionState state() const override;
    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void start() override;

protected:
    virtual void processTask(std::unique_ptr<AbstractExecutor> task);

private:
    std::atomic<ConnectionState> m_state;
    nx::utils::MoveOnlyFunc<void()> m_onClosedHandler;
    nx::utils::thread m_queryExecutionThread;
    std::atomic<bool> m_terminated;
    int m_numberOfFailedRequestsInARow;
    DbConnectionHolder m_dbConnectionHolder;
    const nx::utils::QueueReaderId m_queueReaderId;

    void queryExecutionThreadMain();
    void closeConnection();

    static bool isDbErrorRecoverable(DBResult dbResult);
};

} // namespace db
} // namespace utils
} // namespace nx
