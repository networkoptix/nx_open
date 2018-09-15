#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "basic_tunnel_client.h"
#include "request_paths.h"
#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API GetPostTunnelClient:
    public BasicTunnelClient
{
    using base_type = BasicTunnelClient;

public:
    GetPostTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::utils::Url m_baseTunnelUrl;
    ClientFeedbackFunction m_clientFeedbackFunction;
    nx::utils::Url m_tunnelUrl;
    OpenTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<AsyncClient> m_httpClient;
    Response m_openTunnelResponse;
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
    nx::Buffer m_serializedOpenUpChannelRequest;

    void openDownChannel();

    void onDownChannelOpened();

    void openUpChannel();

    nx::network::http::Request prepareOpenUpChannelRequest();

    void handleOpenUpTunnelResult(
        SystemError::ErrorCode systemErrorCode,
        std::size_t /*bytesTransferred*/);

    void cleanupFailedTunnel();
    void reportFailure(OpenTunnelResult result);

    void reportSuccess();
};

} // namespace nx::network::http::tunneling::detail
