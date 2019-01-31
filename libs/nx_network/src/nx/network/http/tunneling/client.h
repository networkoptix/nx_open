#pragma once

#include <chrono>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "detail/base_tunnel_client.h"
#include "../http_types.h"

namespace nx::network::http::tunneling {

using OpenTunnelResult = detail::OpenTunnelResult;
using OpenTunnelCompletionHandler = detail::OpenTunnelCompletionHandler;

/**
 * Establishes connection to nx::network::http::tunneling::Server listening same request path.
 * For description of tunneling methods supported see nx::network::http::tunneling::Server.
 * The client can try to use different tunneling methods to find the one that works.
 */
class NX_NETWORK_API Client:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    /**
     * @param baseTunnelUrl Path must be equal to that passed to Server().
     */
    Client(const nx::utils::Url& baseTunnelUrl);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::chrono::milliseconds timeout);
    void openTunnel(OpenTunnelCompletionHandler completionHandler);

    const Response& response() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<detail::BaseTunnelClient> m_actualClient;
};

} // namespace nx::network::http::tunneling
