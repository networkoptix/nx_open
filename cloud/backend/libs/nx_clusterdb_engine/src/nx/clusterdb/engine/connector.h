#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/url.h>

#include "transport/abstract_transaction_transport_connector.h"

namespace nx::clusterdb::engine {

class ConnectionManager;

namespace transport { class TransportManager; }

class Connector:
    public nx::network::aio::BasicPollable
{
public:
    Connector(
        transport::TransportManager* transportManager,
        ConnectionManager* connectionManager);

    ~Connector();

    void addNodeUrl(
        const std::string& systemId,
        const nx::utils::Url& url);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct NodeContext
    {
        std::string systemId;
        std::string connectionId;
        std::unique_ptr<transport::AbstractTransactionTransportConnector> connector;
        std::unique_ptr<nx::network::aio::Timer> retryTimer;
    };

    transport::TransportManager* m_transportManager = nullptr;
    ConnectionManager* m_connectionManager = nullptr;
    std::map<nx::utils::Url, NodeContext> m_nodes;
    std::atomic<int> m_connectionSequence{0};

    void connectToNodeAsync(const nx::utils::Url& url);
    
    void onConnectCompletion(
        const nx::utils::Url& url,
        transport::ConnectResultDescriptor result,
        std::unique_ptr<transport::AbstractConnection> connection);
    
    void registerConnection(
        const nx::utils::Url& url,
        const NodeContext& nodeContext,
        std::unique_ptr<transport::AbstractConnection> connection);

    void onConnectionClosed(
        const nx::utils::Url& url,
        SystemError::ErrorCode systemErrorCode);
};

} // namespace nx::clusterdb::engine
