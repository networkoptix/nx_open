#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <utils/db/async_sql_query_executor.h>
#include "persistent_scheduler_db_helper.h"

namespace nx {
namespace db {

class QueryContext;

}
}

namespace nx {
namespace cdb {

class PersistentSheduler;
using ScheduleParamsMap = std::unordered_map<std::string, std::string>;

class AbstractPersistentScheduleEventReceiver
{
public:
    virtual void persistentTimerFired(
        const QnUuid& taskId,
        const ScheduleParamsMap& params,
        nx::db::QueryContext* context) = 0;

    virtual ~AbstractPersistentScheduleEventReceiver() {}
};

class PersistentSheduler
{
    using FunctorToReceiverMap = std::unordered_map<QnUuid, AbstractPersistentScheduleEventReceiver*>;
public:
    PersistentSheduler(
        nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
        AbstractSchedulerDbHelper* dbHelper);

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentScheduleEventReceiver* receiver);

    void start();

    QnUuid subscribe(
        const QnUuid& functorId,
        std::chrono::milliseconds timeout,
        const ScheduleParamsMap& params,
        nx::db::QueryContext* queryContext);

    void unsubscribe(const QnUuid& taskId, nx::db::QueryContext* queryContext);

private:
    nx::db::AbstractAsyncSqlQueryExecutor* m_sqlExecutor;
    AbstractSchedulerDbHelper* m_dbHelper;
    FunctorToReceiverMap m_functorToReceiver;
};

}
}
