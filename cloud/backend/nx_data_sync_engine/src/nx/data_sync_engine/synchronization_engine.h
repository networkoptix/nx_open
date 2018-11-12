#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>

#include "compatible_ec2_protocol_version.h"
#include "connection_manager.h"
#include "connector.h"
#include "dao/rdb/structure_updater.h"
#include "http/sync_connection_request_handler.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "outgoing_command_filter.h"
#include "statistics/provider.h"
#include "transaction_log.h"
#include "transport/http_transport_acceptor.h"
#include "transport/transport_manager.h"
#include "transport/websocket_transport_acceptor.h"

namespace nx {
namespace data_sync_engine {

class SynchronizationSettings;

class NX_DATA_SYNC_ENGINE_API SyncronizationEngine
{
public:
    /**
     * @param supportedProtocolRange Only nodes with compatible protocol
     *     can connect to each other.
     */
    SyncronizationEngine(
        const std::string& applicationId,
        const QnUuid& peerId,
        const SynchronizationSettings& settings,
        const ProtocolVersionRange& supportedProtocolRange,
        nx::sql::AsyncSqlQueryExecutor* const dbManager);
    ~SyncronizationEngine();

    OutgoingTransactionDispatcher& outgoingTransactionDispatcher();
    const OutgoingTransactionDispatcher& outgoingTransactionDispatcher() const;

    TransactionLog& transactionLog();
    const TransactionLog& transactionLog() const;

    IncomingTransactionDispatcher& incomingTransactionDispatcher();
    const IncomingTransactionDispatcher& incomingTransactionDispatcher() const;

    ConnectionManager& connectionManager();
    const ConnectionManager& connectionManager() const;

    Connector& connector();

    const statistics::Provider& statisticsProvider() const;

    void setOutgoingCommandFilter(
        const OutgoingCommandFilterConfiguration& configuration);

    void subscribeToSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);
    void unsubscribeFromSystemDeletedNotification(
        nx::utils::Subscription<std::string>& subscription);

    QnUuid peerId() const;

    void registerHttpApi(
        const std::string& pathPrefix,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

private:
    QnUuid m_peerId;
    OutgoingCommandFilter m_outgoingCommandFilter;
    const ProtocolVersionRange m_supportedProtocolRange;
    OutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    dao::rdb::StructureUpdater m_structureUpdater;
    TransactionLog m_transactionLog;
    IncomingTransactionDispatcher m_incomingTransactionDispatcher;
    ConnectionManager m_connectionManager;
    transport::TransportManager m_transportManager;
    Connector m_connector;
    transport::CommonHttpAcceptor m_httpTransportAcceptor;
    transport::WebSocketTransportAcceptor m_webSocketAcceptor;
    statistics::Provider m_statisticsProvider;
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
