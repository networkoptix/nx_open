#pragma once

#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "detail/basic_tunnel_client.h"
#include "../http_types.h"

namespace nx::network::http::tunneling {

using OpenTunnelResult = detail::OpenTunnelResult;
using OpenTunnelCompletionHandler = detail::OpenTunnelCompletionHandler;

/**
 * Establishes connection to nx::network::http::tunneling::Server listening same request path.
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

    void openTunnel(OpenTunnelCompletionHandler completionHandler);

    const Response& response() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<detail::BasicTunnelClient> m_actualClient;
};

} // namespace nx::network::http::tunneling
