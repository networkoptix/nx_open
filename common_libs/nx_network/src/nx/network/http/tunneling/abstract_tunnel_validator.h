#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "detail/base_tunnel_client.h"

namespace nx_http::tunneling {

using ValidateTunnelCompletionHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

class NX_NETWORK_API AbstractTunnelValidator:
    public nx::network::aio::BasicPollable
{
public:
    virtual void setTimeout(std::chrono::milliseconds timeout) = 0;
    virtual void validate(ValidateTunnelCompletionHandler handler) = 0;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() = 0;
};

} // namespace nx_http::tunneling
