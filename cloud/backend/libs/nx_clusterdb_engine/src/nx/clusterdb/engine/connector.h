#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/subscription.h>
#include <nx/utils/url.h>

#include "transport/abstract_command_transport_connector.h"

namespace nx::clusterdb::engine {

class ConnectionManager;
class SynchronizationSettings;

namespace transport { class TransportManager; }

using OnConnectCompletedSubscription =
    nx::utils::Subscription<const nx::utils::Url&, const transport::ConnectResult&>;

/**
 * Connects local node to other nodes, specified by URLs and makes sure
 * the connection is restored after being broken.
 */
class NX_DATA_SYNC_ENGINE_API Connector:
    public nx::network::aio::BasicPollable
{
public:
    Connector(
        const std::string& nodeId,
        const SynchronizationSettings& settings,
        transport::TransportManager* transportManager,
        ConnectionManager* connectionManager);

    ~Connector();

    void addNodeUrl(
        const std::string& clusterId,
        const nx::utils::Url& url);

    void removeNodeUrl(
        const::std::string& clusterId,
        const nx::utils::Url& url);

    OnConnectCompletedSubscription& onConnectCompletedSubscription();

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct NodeContext
    {
        std::string clusterId;
        std::string connectionId;
        std::string nodeId;
        nx::utils::Url url;
        std::unique_ptr<transport::AbstractTransactionTransportConnector> connector;
        std::unique_ptr<nx::network::aio::Timer> retryTimer;
        nx::utils::SubscriptionId connectionClosedSubscriptionId =
            nx::utils::kInvalidSubscriptionId;
        transport::AbstractConnection* connection = nullptr;
    };

    const std::string m_nodeId;
    const SynchronizationSettings& m_settings;
    transport::TransportManager* m_transportManager = nullptr;
    ConnectionManager* m_connectionManager = nullptr;
    std::map<nx::utils::Url, NodeContext> m_nodes;
    std::atomic<int> m_connectionSequence{0};
    OnConnectCompletedSubscription m_onConnectCompletedSubscription;

    void connectToNodeAsync(const nx::utils::Url& url);

    void scheduleConnectRetry(NodeContext* nodeContext);

    void onConnectCompletion(
        const nx::utils::Url& url,
        transport::ConnectResult result,
        std::unique_ptr<transport::AbstractConnection> connection);

    void registerConnection(
        const nx::utils::Url& url,
        NodeContext* nodeContext,
        std::unique_ptr<transport::AbstractConnection> connection);

    void onConnectionClosed(
        const nx::utils::Url& url,
        SystemError::ErrorCode systemErrorCode);
};

} // namespace nx::clusterdb::engine
