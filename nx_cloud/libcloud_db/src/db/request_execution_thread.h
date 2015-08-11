/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H
#define NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H

#include <memory>

#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>

#include "request_executor.h"


namespace nx {
namespace cdb {
namespace db {


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
        This method MUS be called after class instanciation
        \note Method is needed because we do not use exceptions
    */
    bool open();

protected:
    //!Implementation of QnLongRunnable::run
    virtual void run() override;

private:
    ConnectionOptions m_connectionOptions;
    QSqlDatabase m_dbConnection;
    CLThreadQueue<std::unique_ptr<AbstractExecutor>>* const m_requestQueue;
};


}   //db
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_REQUEST_EXECUTION_THREAD_H
