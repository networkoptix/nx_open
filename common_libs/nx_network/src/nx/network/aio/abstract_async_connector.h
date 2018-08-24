#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "basic_pollable.h"
#include "../socket_common.h"

namespace nx {
namespace network {
namespace aio {

/**
 * NOTE: In general, instance of this class can only be used once.
 */
class NX_NETWORK_API AbstractAsyncConnector:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;

public:
    using ConnectHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connectionToTheTargetPeer*/)>;

    virtual ~AbstractAsyncConnector() = default;

    virtual void connectAsync(
        const network::SocketAddress& targetEndpoint,
        ConnectHandler handler) = 0;
};

} // namespace aio
} // namespace network
} // namespace nx
