#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/vms/api/data/peer_data.h>

namespace nx::data_sync_engine { 

class ConnectionManager;
class ProtocolVersionRange;
class OutgoingCommandFilter;
class TransactionLog;

} // namespace nx::data_sync_engine

namespace nx::data_sync_engine::transport {

class WebSocketTransportAcceptor
{
public:
    WebSocketTransportAcceptor(
        const QnUuid& moduleGuid,
        const ProtocolVersionRange& protocolVersionRange,
        TransactionLog* transactionLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);

    void createConnection(
        nx::network::http::RequestContext requestContext,
        const std::string& systemId,
        nx::network::http::RequestProcessedHandler completionHandler);

private:
    const ProtocolVersionRange& m_protocolVersionRange;
    TransactionLog* m_transactionLog;
    ConnectionManager* m_connectionManager;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const vms::api::PeerData m_localPeerData;

    void addWebSocketTransactionTransport(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        vms::api::PeerDataEx localPeerInfo,
        vms::api::PeerDataEx remotePeerInfo,
        const std::string& systemId,
        const std::string& userAgent);
};

} // namespace nx::data_sync_engine::transport
