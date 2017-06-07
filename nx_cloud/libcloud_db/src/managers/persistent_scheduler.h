#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <utils/db/async_sql_query_executor.h>

namespace nx {
namespace db {

class QueryContext;

}
}

namespace nx {
namespace cdb {

class PersistentSheduler;
using PersistentParamsMap = std::unordered_map<std::string, std::string>;

class AbstractPersistentScheduleEventReceiver
{
public:
    virtual void persistentTimerFired(
        const QnUuid& taskId,
        const PersistentParamsMap& params,
        nx::db::QueryContext* context) = 0;

    virtual ~AbstractPersistentScheduleEventReceiver() {}
};


class PersistentSheduler
{
public:
    PersistentSheduler(nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor);

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentScheduleEventReceiver* receiver);

    void start();

    void subscribe(
        const QnUuid& functorId,
        const QnUuid& taskId,
        std::chrono::milliseconds timeout,
        const PersistentParamsMap& params,
        nx::db::QueryContext* queryContext);

    void unsubscribe(const QnUuid& taskId, nx::db::QueryContext* queryContext);

private:
    std::unordered_map<QnUuid, AbstractPersistentTimerEventReceiver*> m_functorToReceiver;
    std::unordered_map<QnUuid, PersistentParamsMap> m_taskToParams;
    nx::db::AbstractAsyncSqlQueryExecutor* m_sqlExecutor;
};

}
}
