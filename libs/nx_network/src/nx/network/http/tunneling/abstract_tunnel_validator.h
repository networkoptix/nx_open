#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "detail/base_tunnel_client.h"

namespace nx::network::http::tunneling {

using ValidateTunnelCompletionHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

class NX_NETWORK_API AbstractTunnelValidator:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    AbstractTunnelValidator(std::unique_ptr<AbstractStreamSocket> connection);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    std::unique_ptr<AbstractStreamSocket> takeConnection();

    virtual void validate(ValidateTunnelCompletionHandler handler) = 0;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_connection;
};

} // namespace nx::network::http::tunneling
