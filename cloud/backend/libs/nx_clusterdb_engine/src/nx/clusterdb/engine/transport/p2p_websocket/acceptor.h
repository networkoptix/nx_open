#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/vms/api/data/peer_data.h>

namespace nx::clusterdb::engine { 

class ConnectionManager;
class ProtocolVersionRange;
class OutgoingCommandFilter;
class CommandLog;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::p2p::websocket {

class WebSocketTransportAcceptor
{
public:
    WebSocketTransportAcceptor(
        const QnUuid& moduleGuid,
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* transactionLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);

    void registerHandlers(
        const std::string& rootPath,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog;
    ConnectionManager* m_connectionManager;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const vms::api::PeerData m_localPeerData;

    void createConnection(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void addWebSocketTransactionTransport(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        vms::api::PeerDataEx localPeerInfo,
        vms::api::PeerDataEx remotePeerInfo,
        const std::string& systemId,
        const std::string& userAgent);
};

} // namespace nx::clusterdb::engine::transport::p2p::websocket
