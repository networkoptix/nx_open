#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "../../http_async_client.h"
#include "../../http_types.h"

namespace nx::network::http::tunneling::detail {

struct OpenTunnelResult
{
    SystemError::ErrorCode sysError = SystemError::noError;
    StatusCode::Value httpStatus = StatusCode::ok;
    std::unique_ptr<AbstractStreamSocket> connection;
};

using OpenTunnelCompletionHandler =
    nx::utils::MoveOnlyFunc<void(OpenTunnelResult)>;

using ClientFeedbackFunction = nx::utils::MoveOnlyFunc<void(bool /*success*/)>;

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API BasicTunnelClient:
    public aio::BasicPollable
{
public:
    virtual ~BasicTunnelClient() = default;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) = 0;

    virtual const Response& response() const = 0;
};

} // namespace nx::network::http::tunneling::detail
