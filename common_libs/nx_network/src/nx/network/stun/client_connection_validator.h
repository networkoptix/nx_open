#pragma once

#include <optional>

#include <nx/network/http/tunneling/abstract_tunnel_validator.h>

#include "async_client.h"

namespace nx::stun {

/**
 * Validates connection to the server by issuing STUN binding request and waiting for response.
 * Any valid STUN response is considered a success.
 */
class NX_NETWORK_API ClientConnectionValidator:
    public nx_http::tunneling::AbstractTunnelValidator,
    private nx::network::server::StreamConnectionHolder<MessagePipeline>
{
    using base_type = nx_http::tunneling::AbstractTunnelValidator;

public:
    ClientConnectionValidator(
        std::unique_ptr<AbstractStreamSocket> connection);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;
    virtual void validate(nx_http::tunneling::ValidateTunnelCompletionHandler handler) override;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override;

private:
    MessagePipeline m_messagePipeline;
    nx_http::tunneling::ValidateTunnelCompletionHandler m_completionHandler;
    std::unique_ptr<AbstractStreamSocket> m_connection;
    std::optional<std::chrono::milliseconds> m_timeout;

    void sendBindingRequest();
    void processMessage(Message message);
    void processConnectionClosure(SystemError::ErrorCode reasonCode);
};

} // namespace nx::stun
