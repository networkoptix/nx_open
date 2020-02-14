#pragma once

#include <optional>

#include <nx/network/aio/basic_pollable.h>

#include "detail/base_tunnel_client.h"

namespace nx::network::http::tunneling {

using ValidateTunnelCompletionHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

class NX_NETWORK_API AbstractTunnelValidator:
    public aio::BasicPollable
{
public:
    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) = 0;
    virtual void validate(ValidateTunnelCompletionHandler handler) = 0;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() = 0;
};

} // namespace nx::network::http::tunneling
