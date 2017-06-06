#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

namespace nx {
namespace db {

class QueryContext;

}
}

namespace nx {
namespace cdb {

class PersistentTimerManager;
using PersistentParamsMap = std::unordered_map<std::string, std::string>;

class AbstractPersistentTimerEventReceiver
{
public:
    virtual void persistentTimerFired(
        nx::db::QueryContext* queryContext,
        const QnUuid& taskId,
        PersistentParamsMap& params) = 0;

    virtual ~AbstractPersistentTimerEventReceiver() {}
};


class PersistentTimerManager
{
public:
    PersistentTimerManager(nx::db::AbstractAsyncSqlQueryExecutor*);

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentTimerEventReceiver* receiver);

    void start();

    void subscribe(
        const QnUuid& functorId,
        const QnUuid& taskId,
        std::chrono::milliseconds timeout,
        const PersistentParamsMap& params,
        nx::db::QueryContext* queryContext);

    void unsubscribe(const QnUuid& taskId, nx::db::QueryContext* queryContext);

private:
    std::unordered_map<QnUuid, AbstractPersistentTimerEventReceiver*> m_receiverMap;
    std::unordered_map<QnUuid, PersistentParamsMap> m_paramsMap;
};

}
}
