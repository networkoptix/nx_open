#pragma once

#include <list>
#include <map>
#include <memory>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <nx/cloud/cdb/api/maintenance_manager.h>
#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/api/system_data.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_tran_state_data.h>

#include <transaction/connection_guard_shared_state.h>

#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>

#include "serialization/transaction_serializer.h"
#include "transaction_processor.h"
#include "transaction_transport_header.h"
#include "transaction_transport.h"
#include "../access_control/auth_types.h"

namespace nx_http {
class HttpServerConnection;
class MessageDispatcher;
} // namespace nx_http

namespace nx {
namespace cdb {

class AuthorizationManager;

namespace ec2 {

class Settings;

class IncomingTransactionDispatcher;
class OutgoingTransactionDispatcher;
class TransactionLog;
class TransactionTransport;
class TransactionTransportHeader;

/**
 * Manages ec2 transaction connections from mediaservers.
 */
class ConnectionManager
{
public:
    using SystemStatusChangedSubscription =
        nx::utils::Subscription<std::string /*systemId*/, api::SystemHealth>;

    ConnectionManager(
        const QnUuid& moduleGuid,
        const Settings& settings,
        TransactionLog* const transactionLog,
        IncomingTransactionDispatcher* const transactionDispatcher,
        OutgoingTransactionDispatcher* const outgoingTransactionDispatcher);
    virtual ~ConnectionManager();

    /**
     * Mediaserver calls this method to open 2-way transaction exchange channel.
     */
    void createTransactionConnection(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler);
    void createWebsocketTransactionConnection(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler);
    /**
     * Mediaserver uses this method to push transactions.
     */
    void pushTransaction(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler);

    /**
     * Dispatches transaction to corresponding peers.
     */
    void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer);

    api::VmsConnectionDataList getVmsConnections() const;
    std::size_t getVmsConnectionCount() const;
    bool isSystemConnected(const std::string& systemId) const;

    unsigned int getConnectionCountBySystemId(const nx::String& systemId) const;

    void closeConnectionsToSystem(
        const nx::String& systemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    SystemStatusChangedSubscription& systemStatusChangedSubscription();

private:
    class FullPeerName
    {
    public:
        nx::String systemId;
        nx::String peerId;

        bool operator<(const FullPeerName& rhs) const
        {
            return systemId != rhs.systemId
                ? systemId < rhs.systemId
                : peerId < rhs.peerId;
        }
    };

    struct ConnectionContext
    {
        std::unique_ptr<AbstractTransactionTransport> connection;
        nx::String connectionId;
        FullPeerName fullPeerName;
    };

    typedef boost::multi_index::multi_index_container<
        ConnectionContext,
        boost::multi_index::indexed_by<
            // Indexing by connectionId.
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                nx::String,
                &ConnectionContext::connectionId>>,
            // Indexing by (systemId, peerId).
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                FullPeerName,
                &ConnectionContext::fullPeerName>>
        >
    > ConnectionDict;

    constexpr static const int kConnectionByIdIndex = 0;
    constexpr static const int kConnectionByFullPeerNameIndex = 1;

    const Settings& m_settings;
    ::ec2::ConnectionGuardSharedState m_connectionGuardSharedState;
    TransactionLog* const m_transactionLog;
    IncomingTransactionDispatcher* const m_transactionDispatcher;
    OutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    const ::ec2::ApiPeerData m_localPeerData;
    ConnectionDict m_connections;
    mutable QnMutex m_mutex;
    nx::utils::Counter m_startedAsyncCallsCounter;
    nx::utils::SubscriptionId m_onNewTransactionSubscriptionId;
    SystemStatusChangedSubscription m_systemStatusChangedSubscription;

    bool addNewConnection(ConnectionContext connectionContext);

    bool isOneMoreConnectionFromSystemAllowed(
        const QnMutexLockerBase& lk,
        const ConnectionContext& context) const;

    unsigned int getConnectionCountBySystemId(
        const QnMutexLockerBase& lk,
        const nx::String& systemId) const;

    template<int connectionIndexNumber, typename ConnectionKeyType>
        void removeExistingConnection(
            const QnMutexLockerBase& /*lock*/,
            ConnectionKeyType connectionKey);

    template<typename ConnectionIndex, typename Iterator, typename CompletionHandler>
    void removeConnectionByIter(
        const QnMutexLockerBase& /*lock*/,
        ConnectionIndex& connectionIndex,
        Iterator connectionIterator,
        CompletionHandler completionHandler);

    void sendSystemOfflineNotificationIfNeeded(const nx::String systemId);

    void removeConnection(const nx::String& connectionId);

    void onGotTransaction(
        const nx::String& connectionId,
        Qn::SerializationFormat tranFormat,
        QByteArray serializedTransaction,
        TransactionTransportHeader transportHeader);

    void onTransactionDone(const nx::String& connectionId, api::ResultCode resultCode);

    bool fetchDataFromConnectRequest(
        const nx_http::Request& request,
        ConnectionRequestAttributes* connectionRequestAttributes);

    template<typename TransactionDataType>
    void processSpecialTransaction(
        const nx::String& systemId,
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTransaction<TransactionDataType> data,
        TransactionProcessedHandler handler);

    nx_http::RequestResult prepareOkResponseToCreateTransactionConnection(
        const ConnectionRequestAttributes& connectionRequestAttributes,
        nx_http::Response* const response);

    void onHttpConnectionUpgraded(
        nx_http::HttpServerConnection* connection,
        ::ec2::ApiPeerDataEx remotePeerInfo,
        nx::utils::stree::ResourceContainer authInfo,
        const nx::String systemId);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
