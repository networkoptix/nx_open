#pragma once

#include <list>
#include <map>
#include <memory>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/peer_data.h>

#include <transaction/connection_guard_shared_state.h>

#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>

#include "serialization/transaction_serializer.h"
#include "transaction_processor.h"
#include "transaction_transport_header.h"
#include "transaction_transport.h"

namespace nx {
namespace network {
namespace http {

class HttpServerConnection;
class MessageDispatcher;

} // namespace nx
} // namespace network
} // namespace http

namespace nx {
namespace data_sync_engine {

class Settings;

class IncomingTransactionDispatcher;
class OutgoingTransactionDispatcher;
class TransactionLog;
class TransactionTransport;
class TransactionTransportHeader;

struct SystemStatusDescriptor
{
    bool isOnline = false;
    int protoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;

    SystemStatusDescriptor() = default;
};

struct SystemConnectionInfo
{
    std::string systemId;
    nx::network::SocketAddress peerEndpoint;
    std::string userAgent;
};

/**
 * Manages ec2 transaction connections from mediaservers.
 */
class NX_DATA_SYNC_ENGINE_API ConnectionManager
{
public:
    using SystemStatusChangedSubscription =
        nx::utils::Subscription<std::string /*systemId*/, SystemStatusDescriptor>;

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
        nx::network::http::HttpServerConnection* const connection,
        const std::string& systemId,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);
    void createWebsocketTransactionConnection(
        nx::network::http::HttpServerConnection* const connection,
        const std::string& systemId,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);
    /**
     * Mediaserver uses this method to push transactions.
     */
    void pushTransaction(
        nx::network::http::HttpServerConnection* const connection,
        const std::string& systemId,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);

    /**
     * Dispatches transaction to corresponding peers.
     */
    void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer);

    std::vector<SystemConnectionInfo> getConnections() const;
    std::size_t getConnectionCount() const;
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
        std::string userAgent;
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
    const vms::api::PeerData m_localPeerData;
    ConnectionDict m_connections;
    mutable QnMutex m_mutex;
    nx::utils::Counter m_startedAsyncCallsCounter;
    nx::utils::SubscriptionId m_onNewTransactionSubscriptionId;
    SystemStatusChangedSubscription m_systemStatusChangedSubscription;

    bool addNewConnection(
        ConnectionContext connectionContext,
        const vms::api::PeerDataEx& remotePeerInfo);

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

    void onTransactionDone(const nx::String& connectionId, ResultCode resultCode);

    bool fetchDataFromConnectRequest(
        const nx::network::http::Request& request,
        ConnectionRequestAttributes* connectionRequestAttributes);

    template<typename TransactionDataType>
    void processSpecialTransaction(
        const nx::String& systemId,
        const TransactionTransportHeader& transportHeader,
        Command<TransactionDataType> data,
        TransactionProcessedHandler handler);

    nx::network::http::RequestResult prepareOkResponseToCreateTransactionConnection(
        const ConnectionRequestAttributes& connectionRequestAttributes,
        nx::network::http::Response* const response);

    void addWebSocketTransactionTransport(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        vms::api::PeerDataEx remotePeerInfo,
        const std::string& systemId,
        const std::string& userAgent);
};

} // namespace data_sync_engine
} // namespace nx
