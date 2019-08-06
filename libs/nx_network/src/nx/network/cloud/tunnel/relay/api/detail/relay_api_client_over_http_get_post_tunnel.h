#pragma once

#include <list>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/auth_cache.h>
#include <nx/network/http/fusion_data_http_client.h>

#include "get_post_tunnel_context.h"
#include "relay_api_client_over_http_upgrade.h"

namespace nx::cloud::relay::api::detail {

class NX_NETWORK_API ClientOverHttpGetPostTunnel:
    public ClientOverHttpUpgrade
{
    using base_type = ClientOverHttpUpgrade;

public:
    ClientOverHttpGetPostTunnel(
        const nx::utils::Url& baseUrl,
        ClientFeedbackFunction feedbackFunction);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void beginListening(
        const std::string& peerName,
        BeginListeningHandler completionHandler) override;

    virtual void openConnectionToTheTargetHost(
        const std::string& sessionId,
        OpenRelayConnectionHandler handler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    using Tunnels = std::list<std::unique_ptr<detail::TunnelContext>>;

    Tunnels m_tunnelsBeingEstablished;

    template<typename TunnelContext, typename UserHandler>
    void openDownChannel(
        const nx::utils::Url& tunnelUrl,
        UserHandler completionHandler);

    void onDownChannelOpened(Tunnels::iterator tunnelCtxIter);

    void openUpChannel(Tunnels::iterator tunnelCtxIter);

    nx::network::http::Request prepareOpenUpChannelRequest(
        Tunnels::iterator tunnelCtxIter);

    void handleOpenUpTunnelResult(
        Tunnels::iterator tunnelCtxIter,
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesTransferreded);

    void cleanUpFailedTunnel(Tunnels::iterator tunnelCtxIter);

    void reportSuccess(Tunnels::iterator tunnelCtxIter);
};

} // namespace nx::cloud::relay::api::detail
