#pragma once

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/http/asynchttpclient.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * Initiates NXRC/1.0 connection over HTTP upgrade
 */
class NX_NETWORK_API ReverseConnector
{
public:
    ReverseConnector(
        String selfHostName, String targetHostName, aio::AbstractAioThread* aioThread);

    void setHttpTimeouts(nx_http::AsyncHttpClient::Timeouts timeouts);

    // TODO: MoveOnlyFunc when HttpClient supports it
    typedef std::function<void(SystemError::ErrorCode)> ConnectHandler;
    void connect(const SocketAddress& endpoint, ConnectHandler handler);

    std::unique_ptr<BufferedStreamSocket> takeSocket();
    boost::optional<size_t> getPoolSize() const;
    boost::optional<KeepAliveOptions> getKeepAliveOptions() const;

private:
    SystemError::ErrorCode processHeaders(const nx_http::HttpHeaders& headers);

    const String m_targetHostName;
    nx_http::AsyncHttpClientPtr m_httpClient;
    std::function<void(SystemError::ErrorCode)> m_handler;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
