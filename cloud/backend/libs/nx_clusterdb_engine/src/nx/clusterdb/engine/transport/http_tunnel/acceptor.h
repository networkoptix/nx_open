#pragma once

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/tunneling/abstract_tunnel_authorizer.h>
#include <nx/network/http/tunneling/server.h>
#include <nx/utils/uuid.h>

#include <nx/vms/api/data/peer_data.h>

#include "../abstract_acceptor.h"
#include "../abstract_command_transport.h"

namespace nx::clusterdb::engine {

class ConnectionManager;
class ProtocolVersionRange;
class OutgoingCommandFilter;
class CommandLog;

} // namespace nx::clusterdb::engine

namespace nx::clusterdb::engine::transport::http_tunnel {

namespace detail {

struct ConnectRequestContext
{
    std::string clusterId;
    ConnectionRequestAttributes attributes;
    std::string userAgent;
};

} // namespace detail

class Acceptor:
    public AbstractAcceptor,
    private network::http::tunneling::TunnelAuthorizer<detail::ConnectRequestContext>
{
    using base_type =
        network::http::tunneling::TunnelAuthorizer<detail::ConnectRequestContext>;

public:
    Acceptor(
        const QnUuid& peerId,
        const ProtocolVersionRange& protocolVersionRange,
        CommandLog* transactionLog,
        ConnectionManager* connectionManager,
        const OutgoingCommandFilter& outgoingCommandFilter);

    virtual void registerHandlers(
        const std::string& rootPath,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher) override;

private:
    const QnUuid m_peerId;
    const ProtocolVersionRange& m_protocolVersionRange;
    CommandLog* m_commandLog = nullptr;
    ConnectionManager* m_connectionManager = nullptr;
    const OutgoingCommandFilter& m_outgoingCommandFilter;
    const vms::api::PeerData m_localPeerData;
    nx::network::http::tunneling::Server<detail::ConnectRequestContext> m_tunnelingServer;

    virtual void authorize(
        const network::http::RequestContext* requestContext,
        CompletionHandler completionHandler) override;

    void saveCreatedTunnel(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        detail::ConnectRequestContext connectRequestContext);
};

} // namespace nx::clusterdb::engine::transport::http_tunnel
