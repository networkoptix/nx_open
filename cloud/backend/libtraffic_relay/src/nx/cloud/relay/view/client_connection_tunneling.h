#pragma once

#include <nx/network/http/tunneling/server.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "../controller/connect_session_manager.h"

namespace nx::cloud::relay::view {

class ClientConnectionTunnelingServer:
    public nx::network::http::tunneling::TunnelAuthorizer<
        controller::AbstractConnectSessionManager::StartRelayingFunc>
{
public:
    ClientConnectionTunnelingServer(
        controller::AbstractConnectSessionManager* connectSessionManager);

    void registerHandlers(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher);

protected:
    virtual void authorize(
        const nx::network::http::RequestContext* requestContext,
        CompletionHandler completionHandler) override;

private:
    using TunnelingServer =
        nx::network::http::tunneling::Server<
            controller::AbstractConnectSessionManager::StartRelayingFunc>;

    controller::AbstractConnectSessionManager* m_connectSessionManager = nullptr;
    TunnelingServer m_tunnelingServer;

    void onConnectToPeerFinished(
        api::ResultCode resultCode,
        network::http::Response* httpResponse,
        controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
        CompletionHandler completionHandler);

    void saveNewTunnel(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc);
};

} // namespace nx::cloud::relay::view
