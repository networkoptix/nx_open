#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/std/optional.h>

#include "base_tunnel_client.h"
#include "request_paths.h"
#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API GetPostTunnelClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    GetPostTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

private:
    nx::utils::Url m_tunnelUrl;
    Response m_openTunnelResponse;
    nx::Buffer m_serializedOpenUpChannelRequest;
    std::optional<std::chrono::milliseconds> m_timeout;

    void openDownChannel();

    void onDownChannelOpened();

    void openUpChannel();

    nx::network::http::Request prepareOpenUpChannelRequest();

    void handleOpenUpTunnelResult(
        SystemError::ErrorCode systemErrorCode,
        std::size_t /*bytesTransferred*/);
};

} // namespace nx::network::http::tunneling::detail
