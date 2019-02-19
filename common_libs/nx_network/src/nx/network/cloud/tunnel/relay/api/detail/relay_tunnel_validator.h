#pragma once

#include <optional>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/tunneling/abstract_tunnel_validator.h>

namespace nx::cloud::relay::api::detail {

class TunnelValidator:
    public nx_http::tunneling::AbstractTunnelValidator,
    private network::server::StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
    using base_type = nx_http::tunneling::AbstractTunnelValidator;

public:
    TunnelValidator(
        std::unique_ptr<AbstractStreamSocket> connection,
        const nx_http::Response& response);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void setTimeout(std::chrono::milliseconds timeout) override;
    virtual void validate(nx_http::tunneling::ValidateTunnelCompletionHandler handler) override;
    virtual std::unique_ptr<AbstractStreamSocket> takeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx_http::AsyncMessagePipeline m_httpConnection;
    std::unique_ptr<AbstractStreamSocket> m_connection;
    std::optional<std::chrono::milliseconds> m_timeout;
    std::string m_relayProtocolVersion;
    nx_http::tunneling::ValidateTunnelCompletionHandler m_handler;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::AsyncMessagePipeline* connection) override;

    void fetchProtocolVersion(const nx_http::Response& response);
    bool relaySupportsKeepAlive() const;
    void processRelayNotification(nx_http::Message message);
    void handleConnectionClosure(SystemError::ErrorCode reason);
};

} // namespace nx::cloud::relay::api::detail
