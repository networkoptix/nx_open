#pragma once

#include <nx/clusterdb/engine/incoming_command_dispatcher.h>
#include <nx/clusterdb/engine/command_log.h>
#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/sql/query_context.h>

#include "nx/cloud/storage/service/settings.h"
#include "nx/cloud/storage/service/model/database.h"

namespace nx::cloud::storage::service::controller {

class BaseManager
{

public:
    BaseManager(
        const std::string& clusterId,
        nx::clusterdb::engine::SynchronizationEngine* syncEngine)
        :
        m_clusterId(clusterId),
        m_syncEngine(syncEngine)
    {
    }

protected:
    template<typename Command, typename DbFunc>
    void registerCommandHandler(DbFunc dbFunc)
    {
        m_syncEngine->incomingCommandDispatcher()
            .registerCommandHandler<Command>(
                [dbFunc = std::move(dbFunc)](auto queryContext, auto /*clusterId*/, auto command)
                {
                    dbFunc(queryContext, command.params);
                    return nx::sql::DBResult::ok;
                });
    }

    template<typename Command, typename DbFunc>
    nx::sql::DBResult manipulateDbAndSynchronize(
        nx::sql::QueryContext* queryContext,
        const typename Command::Data& data,
        DbFunc dbFunc)
    {
        dbFunc(queryContext, data);

        m_syncEngine->transactionLog().generateTransactionAndSaveToLog<Command>(
            queryContext,
            m_clusterId,
            data);

        return nx::sql::DBResult::ok;
    }

private:
    const std::string& m_clusterId;
    nx::clusterdb::engine::SynchronizationEngine* m_syncEngine;
};

} // namespace nx::cloud::storage::service::controller