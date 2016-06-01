/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H
#define NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H

#include <atomic>
#include <memory>

#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>

#include "request_executor.h"


namespace nx {
namespace db {

/**
    Connection can be closed by timeout or due to error. 
    Use \a DbRequestExecutionThread::isOpen to test it
*/
class DbRequestExecutionThread
:
    public QnLongRunnable
{
public:
    DbRequestExecutionThread(
        const ConnectionOptions& connectionOptions,
        CLThreadQueue<std::unique_ptr<AbstractExecutor>>* const requestQueue );
    virtual ~DbRequestExecutionThread();

    //!Establishes connection to DB
    /*!
        This method MUST be called after class instanciation
        \note Method is needed because we do not use exceptions
    */
    bool open();
    bool isOpen() const;

protected:
    //!Implementation of QnLongRunnable::run
    virtual void run() override;

private:
    ConnectionOptions m_connectionOptions;
    QSqlDatabase m_dbConnection;
    CLThreadQueue<std::unique_ptr<AbstractExecutor>>* const m_requestQueue;
    std::atomic<bool> m_isOpen;
};

}   //db
}   //nx

#endif  //NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H
