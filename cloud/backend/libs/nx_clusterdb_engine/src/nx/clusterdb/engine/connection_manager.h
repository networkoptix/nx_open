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
#include <nx/utils/counter.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

#include <nx/vms/api/data/peer_data.h>

#include "compatible_ec2_protocol_version.h"
#include "serialization/command_serializer.h"
#include "command_processor.h"
#include "transport/command_transport_header.h"
#include "transport/abstract_command_transport.h"

namespace nx::network::http {

class HttpServerConnection;
class MessageDispatcher;

} // namespace nx::network::http

namespace nx::clusterdb::engine {

class SynchronizationSettings;

class IncomingCommandDispatcher;
class OutgoingCommandDispatcher;
class CommandLog;
class CommandTransportHeader;

struct NodeStatusDescriptor
{
    bool isOnline = false;
    int protoVersion = 0;

    NodeStatusDescriptor() = delete;
};

struct FullPeerName
{
    std::string clusterId;
    std::string peerId;

    bool operator<(const FullPeerName& rhs) const
    {
        return clusterId != rhs.clusterId
            ? clusterId < rhs.clusterId
            : peerId < rhs.peerId;
    }

    std::string toString() const
    {
        return peerId + "." + clusterId;
    }
};

struct NodeDescriptor
{
    FullPeerName name;
};

struct ConnectionInfo
{
    std::string clusterId;
    std::string nodeId;
    nx::network::SocketAddress peerEndpoint;
    std::string userAgent;
};

/**
 * Manages ec2 transaction connections from mediaservers.
 */
class NX_DATA_SYNC_ENGINE_API ConnectionManager
{
public:
    struct ConnectionContext
    {
        std::unique_ptr<transport::AbstractConnection> connection;
        std::string originatingNodeId;
        std::string connectionId;
        FullPeerName fullPeerName;
        std::string userAgent;
    };

    using ClusterStatusChangedSubscription =
        nx::utils::Subscription<std::string /*clusterId*/, NodeStatusDescriptor>;

    using NodeStatusChangedSubscription =
        nx::utils::Subscription<NodeDescriptor, NodeStatusDescriptor>;

    ConnectionManager(
        const std::string& nodeId,
        const SynchronizationSettings& settings,
        const ProtocolVersionRange& protocolVersionRange,
        IncomingCommandDispatcher* const transactionDispatcher,
        OutgoingCommandDispatcher* const outgoingTransactionDispatcher);
    virtual ~ConnectionManager();

    void pleaseStopSync();

    bool addNewConnection(ConnectionContext connectionContext);

    void removeConnection(const std::string& connectionId);

    /**
     * @return false If connectionId does not refer to an existing connection.
     */
    bool modifyConnectionByIdSafe(
        const std::string& connectionId,
        std::function<void(transport::AbstractConnection* connection)> func);

    /**
     * Dispatches transaction to corresponding peers.
     */
    void dispatchTransaction(
        const std::string& clusterId,
        std::shared_ptr<const SerializableAbstractCommand> transactionSerializer);

    std::vector<ConnectionInfo> getConnections() const;
    std::size_t getConnectionCount() const;
    bool isClusterConnected(const std::string& clusterId) const;

    bool isNodeConnected(
        const std::string& clusterId,
        const std::string& nodeId) const;

    unsigned int getConnectionCountByClusterId(const std::string& clusterId) const;

    void closeConnectionsToCluster(
        const std::string& clusterId,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    ClusterStatusChangedSubscription& clusterStatusChangedSubscription();
    NodeStatusChangedSubscription& nodeStatusChangedSubscription();

private:
    typedef boost::multi_index::multi_index_container<
        ConnectionContext,
        boost::multi_index::indexed_by<
            // Indexing by connectionId.
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                std::string,
                &ConnectionContext::connectionId>>,
            // Indexing by (clusterId, peerId).
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                FullPeerName,
                &ConnectionContext::fullPeerName>>
        >
    > ConnectionDict;

    constexpr static const int kConnectionByIdIndex = 0;
    constexpr static const int kConnectionByFullPeerNameIndex = 1;

    const std::string m_nodeId;
    const SynchronizationSettings& m_settings;
    const ProtocolVersionRange m_protocolVersionRange;
    IncomingCommandDispatcher* const m_transactionDispatcher;
    OutgoingCommandDispatcher* const m_outgoingTransactionDispatcher;
    const vms::api::PeerData m_localPeerData;
    ConnectionDict m_connections;
    mutable QnMutex m_mutex;
    nx::utils::Counter m_startedAsyncCallsCounter;
    nx::utils::SubscriptionId m_onNewTransactionSubscriptionId;
    ClusterStatusChangedSubscription m_clusterStatusChangedSubscription;
    NodeStatusChangedSubscription m_nodeStatusChangedSubscription;

    bool authorizeNewConnection(
        const QnMutexLockerBase& lk,
        const ConnectionContext& context);

    bool isOneMoreConnectionFromClusterAllowed(
        const QnMutexLockerBase& lk,
        const ConnectionContext& context) const;

    unsigned int getConnectionCountByClusterId(
        const QnMutexLockerBase& lk,
        const std::string& clusterId) const;

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

    void sendClusterOfflineNotificationIfNeeded(const CommandTransportHeader& transportHeader);

    void onGotTransaction(
        const std::string& connectionId,
        std::unique_ptr<DeserializableCommandData> commandData,
        CommandTransportHeader transportHeader);

    void onTransactionDone(const std::string& connectionId, ResultCode resultCode);
};

} // namespace nx::clusterdb::engine
