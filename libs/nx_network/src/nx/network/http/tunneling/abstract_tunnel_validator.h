#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "detail/base_tunnel_client.h"

namespace nx::network::http::tunneling {

using ValidateTunnelCompletionHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

class NX_NETWORK_API AbstractTunnelValidator:
    public aio::BasicPollable
{
public:
    virtual void validate(ValidateTunnelCompletionHandler handler) = 0;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() = 0;
};

} // namespace nx::network::http::tunneling
