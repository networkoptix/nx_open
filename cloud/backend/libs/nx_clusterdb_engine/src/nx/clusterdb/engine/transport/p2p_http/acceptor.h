#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/utils/uuid.h>

#include <nx/vms/api/data/peer_data.h>

namespace nx::clusterdb::engine {

class ProtocolVersionRange;
class CommandLog;
class ConnectionManager;
class OutgoingCommandFilter;

} // nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::p2p::http {

class Acceptor
{
public:
    Acceptor(
        const std::string& nodeId,
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* commandLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);
    
    void registerHandlers(
        const std::string& rootPath,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    ConnectionManager* m_connectionManager = nullptr;
    const OutgoingCommandFilter& m_commandFilter;
    const vms::api::PeerData m_localPeer;

    void openServerCommandChannel(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void processClientCommand(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    bool validateRequest(
        const nx::network::http::RequestContext& requestContext,
        const std::string& connectionId,
        const std::string& systemId,
        const vms::api::PeerDataEx& peerInfo);

    bool registerNewConnection(
        const nx::network::http::RequestContext& requestContext,
        const std::string& connectionId,
        const std::string& systemId,
        std::unique_ptr<nx::p2p::IP2PTransport> commandPipeline,
        const vms::api::PeerDataEx& localPeer,
        const vms::api::PeerDataEx& remotePeer);
};

} // namespace nx::clusterdb::engine::transport::p2p::http
