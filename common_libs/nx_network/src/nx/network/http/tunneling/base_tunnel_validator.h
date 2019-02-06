#pragma once

#include "abstract_tunnel_validator.h"

namespace nx_http::tunneling {

class NX_NETWORK_API BaseTunnelValidator:
    public AbstractTunnelValidator
{
    using base_type = AbstractTunnelValidator;

public:
    BaseTunnelValidator(std::unique_ptr<AbstractStreamSocket> connection);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_connection;
};

} // namespace nx_http::tunneling
