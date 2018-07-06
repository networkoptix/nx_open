#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>

#include "connection_manager.h"
#include "dao/rdb/structure_updater.h"
#include "http/sync_connection_request_handler.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "transaction_log.h"

namespace nx {
namespace data_sync_engine {

class Settings;

class NX_DATA_SYNC_ENGINE_API SyncronizationEngine
{
public:
    SyncronizationEngine(
        const QnUuid& moduleGuid,
        const Settings& settings,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager);
    ~SyncronizationEngine();

    OutgoingTransactionDispatcher& outgoingTransactionDispatcher();
    const OutgoingTransactionDispatcher& outgoingTransactionDispatcher() const;

    TransactionLog& transactionLog();
    const TransactionLog& transactionLog() const;

    IncomingTransactionDispatcher& incomingTransactionDispatcher();
    const IncomingTransactionDispatcher& incomingTransactionDispatcher() const;

    ConnectionManager& connectionManager();
    const ConnectionManager& connectionManager() const;

    void subscribeToSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);
    void unsubscribeFromSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);

    void registerHttpApi(
        const std::string& pathPrefix,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

private:
    OutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    dao::rdb::StructureUpdater m_structureUpdater;
    TransactionLog m_transactionLog;
    IncomingTransactionDispatcher m_incomingTransactionDispatcher;
    ConnectionManager m_connectionManager;
    nx::utils::SubscriptionId m_systemDeletedSubscriptionId;
    nx::utils::Counter m_startedAsyncCallsCounter;

    template<typename ManagerType>
    void registerHttpHandler(
        const std::string& handlerPath,
        typename SyncConnectionRequestHandler<ManagerType>::ManagerFuncType managerFuncPtr,
        ManagerType* manager,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

    template<typename ManagerType>
    void registerHttpHandler(
        nx::network::http::Method::ValueType method,
        const std::string& handlerPath,
        typename SyncConnectionRequestHandler<ManagerType>::ManagerFuncType managerFuncPtr,
        ManagerType* manager,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

    void onSystemDeleted(const std::string& systemId);
};

} // namespace data_sync_engine
} // namespace nx
