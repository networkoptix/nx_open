#pragma once

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/tunneling/server.h>

#include <nx/utils/uuid.h>

namespace nx::data_sync_engine::transport {

class ConnectionManager;
class ProtocolVersionRange;
class OutgoingCommandFilter;
class TransactionLog;

class HttpTunnelTransportAcceptor
{
public:
    HttpTunnelTransportAcceptor(
        const QnUuid& peerId,
        const ProtocolVersionRange& protocolVersionRange,
        TransactionLog* transactionLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);

    void registerHandlers(
        const std::string& rootPath,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

private:
    const QnUuid m_peerId;
    //const ProtocolVersionRange& m_protocolVersionRange;
    //TransactionLog* m_transactionLog = nullptr;
    //ConnectionManager* m_connectionManager = nullptr;
    //const OutgoingCommandFilter& m_outgoingCommandFilter;

    nx::network::http::tunneling::Server<> m_tunnelingServer;

    void saveCreatedTunnel(
        std::unique_ptr<network::AbstractStreamSocket> connection);
};

} // namespace nx::data_sync_engine::transport
