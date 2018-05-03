#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/buffered_stream_socket.h>
#include <nx/network/http/http_async_client.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

// TODO: #ak Inherit aio::BasicPollable?

/**
 * Initiates NXRC/1.0 connection over HTTP upgrade.
 * It is only safe to remove this object when async operations are in progress or from bound AIO
 * thread (see Constructor).
 */
class NX_NETWORK_API ReverseConnector:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    // TODO: MoveOnlyFunc when HttpClient supports it
    using ConnectHandler = std::function<void(SystemError::ErrorCode)>;

    ReverseConnector(
        String selfHostName, String targetHostName);

    ReverseConnector(const ReverseConnector&) = delete;
    ReverseConnector(ReverseConnector&&) = delete;
    ReverseConnector& operator=(const ReverseConnector&) = delete;
    ReverseConnector& operator=(ReverseConnector&&) = delete;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setHttpTimeouts(nx::network::http::AsyncClient::Timeouts timeouts);

    void connect(const SocketAddress& endpoint, ConnectHandler handler);

    std::unique_ptr<BufferedStreamSocket> takeSocket();
    boost::optional<size_t> getPoolSize() const;
    boost::optional<KeepAliveOptions> getKeepAliveOptions() const;

private:
    const String m_targetHostName;
    nx::network::http::AsyncClient m_httpClient;
    ConnectHandler m_handler;

    virtual void stopWhileInAioThread() override;

    SystemError::ErrorCode processHeaders(const nx::network::http::HttpHeaders& headers);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
