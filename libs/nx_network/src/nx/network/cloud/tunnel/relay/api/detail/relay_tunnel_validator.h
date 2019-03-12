#pragma once

#include <optional>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/tunneling/abstract_tunnel_validator.h>

namespace nx::cloud::relay::api::detail {

class TunnelValidator:
    public network::http::tunneling::AbstractTunnelValidator
{
    using base_type = network::http::tunneling::AbstractTunnelValidator;

public:
    TunnelValidator(
        std::unique_ptr<network::AbstractStreamSocket> connection,
        const network::http::Response& response);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;
    virtual void validate(network::http::tunneling::ValidateTunnelCompletionHandler handler) override;
    virtual std::unique_ptr<network::AbstractStreamSocket> takeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    network::http::AsyncMessagePipeline m_httpConnection;
    std::unique_ptr<network::AbstractStreamSocket> m_connection;
    std::optional<std::chrono::milliseconds> m_timeout;
    std::string m_relayProtocolVersion;
    network::http::tunneling::ValidateTunnelCompletionHandler m_handler;

    void fetchProtocolVersion(const network::http::Response& response);
    bool relaySupportsKeepAlive() const;
    void processRelayNotification(network::http::Message message);
    void handleConnectionClosure(SystemError::ErrorCode reason);
};

} // namespace nx::cloud::relay::api::detail
