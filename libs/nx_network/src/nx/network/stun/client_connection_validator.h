#pragma once

#include <nx/network/http/tunneling/abstract_tunnel_validator.h>

#include "async_client.h"

namespace nx::network::stun {

/**
 * Validates connection to the server by issuing STUN binding request and waiting for response.
 * Any valid STUN response is considered a success.
 */
class NX_NETWORK_API ClientConnectionValidator:
    public http::tunneling::AbstractTunnelValidator
{
    using base_type = http::tunneling::AbstractTunnelValidator;

public:
    ClientConnectionValidator(
        std::unique_ptr<AbstractStreamSocket> connection);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void validate(http::tunneling::ValidateTunnelCompletionHandler handler) override;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    MessagePipeline m_messagePipeline;
    http::tunneling::ValidateTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<AbstractStreamSocket> m_connection;

    void sendBindingRequest();
    void processMessage(Message message);
    void processConnectionClosure(SystemError::ErrorCode reasonCode);
};

} // namespace nx::network::stun
