#pragma once

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/http/asynchttpclient.h>

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
class NX_NETWORK_API ReverseConnector
{
public:
    // TODO: MoveOnlyFunc when HttpClient supports it
    using ConnectHandler = std::function<void(SystemError::ErrorCode)>;

    ReverseConnector(
        String selfHostName, String targetHostName, aio::AbstractAioThread* aioThread);

    ReverseConnector(const ReverseConnector&) = delete;
    ReverseConnector(ReverseConnector&&) = delete;
    ReverseConnector& operator=(const ReverseConnector&) = delete;
    ReverseConnector& operator=(ReverseConnector&&) = delete;

    void setHttpTimeouts(nx_http::AsyncHttpClient::Timeouts timeouts);

    void connect(const SocketAddress& endpoint, ConnectHandler handler);

    std::unique_ptr<BufferedStreamSocket> takeSocket();
    boost::optional<size_t> getPoolSize() const;
    boost::optional<KeepAliveOptions> getKeepAliveOptions() const;

private:
    SystemError::ErrorCode processHeaders(const nx_http::HttpHeaders& headers);

    const String m_targetHostName;
    nx_http::AsyncHttpClientPtr m_httpClient;
    ConnectHandler m_handler;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
