#pragma once

#include <list>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/auth_cache.h>
#include <nx/network/http/fusion_data_http_client.h>

//#include "relay_api_basic_client.h"
#include "relay_api_client_over_http_upgrade.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API ClientOverHttpGetPostTunnel:
    public ClientOverHttpUpgrade/*BasicClient*/
{
    using base_type = ClientOverHttpUpgrade/*BasicClient*/;

public:
    ClientOverHttpGetPostTunnel(
        const nx::utils::Url& baseUrl,
        ClientFeedbackFunction /*feedbackFunction*/);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void beginListening(
        const std::string& peerName,
        BeginListeningHandler completionHandler) override;

    //virtual void openConnectionToTheTargetHost(
    //    const std::string& sessionId,
    //    OpenRelayConnectionHandler handler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct TunnelContext
    {
        nx::utils::Url tunnelUrl;
        BeginListeningHandler userHandler;
        std::unique_ptr<nx::network::http::AsyncClient> httpClient;
        BeginListeningResponse beginListeningResponse;
        std::unique_ptr<network::AbstractStreamSocket> connection;
        nx::Buffer serializedOpenUpChannelRequest;
    };

    std::list<TunnelContext> m_tunnelsBeingEstablished;

    void openDownChannel(
        const nx::utils::Url& tunnelUrl,
        BeginListeningHandler completionHandler);

    void onDownChannelOpened(
        std::list<TunnelContext>::iterator tunnelCtxIter);

    void openUpChannel(
        std::list<TunnelContext>::iterator tunnelCtxIter);

    nx::network::http::Request prepareOpenUpChannelRequest(
        std::list<TunnelContext>::iterator tunnelCtxIter);

    void handleOpenUpTunnelResult(
        std::list<TunnelContext>::iterator tunnelCtxIter,
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesTransferreded);

    void cleanupFailedTunnel(
        std::list<TunnelContext>::iterator tunnelCtxIter);

    void reportSuccess(
        std::list<TunnelContext>::iterator tunnelCtxIter);
};

} // namespace nx::cloud::relay::api
