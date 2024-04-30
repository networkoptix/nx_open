// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/std/optional.h>

#include "../../http_async_client.h"
#include "../../http_types.h"
#include "base_tunnel_client.h"
#include "request_paths.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API GetPostTunnelClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    GetPostTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        const ConnectOptions& options,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

private:
    nx::utils::Url m_tunnelUrl;
    Response m_openTunnelResponse;
    nx::Buffer m_serializedOpenUpChannelRequest;
    std::optional<std::chrono::milliseconds> m_timeout;
    bool m_isConnectionTestRequested = false;

    void openDownChannel();

    void onDownChannelOpened();

    void openUpChannel();

    nx::network::http::Request prepareOpenUpChannelRequest();

    void handleOpenUpTunnelResult(
        SystemError::ErrorCode systemErrorCode,
        std::size_t /*bytesTransferred*/);
};

} // namespace nx::network::http::tunneling::detail
