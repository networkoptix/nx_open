#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "abstract_authentication_manager.h"
#include "http_message_dispatcher.h"
#include "http_server_connection.h"
#include "http_statistics.h"

namespace nx {
namespace network {
namespace http {

class NX_NETWORK_API HttpStreamSocketServer:
    public nx::network::server::StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>,
    public nx::network::http::server::AbstractHttpStatisticsProvider
{
    using base_type =
        nx::network::server::StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>;

public:
    using ConnectionType = HttpServerConnection;

    template<typename ... StreamServerArgs>
    HttpStreamSocketServer(
        nx::network::http::server::AbstractAuthenticationManager* const authenticationManager,
        nx::network::http::AbstractMessageDispatcher* const httpMessageDispatcher,
        StreamServerArgs... streamServerArgs)
        :
        base_type(std::move(streamServerArgs)...),
        m_authenticationManager(authenticationManager),
        m_httpMessageDispatcher(httpMessageDispatcher),
        m_persistentConnectionEnabled(true)
    {
    }

    virtual ~HttpStreamSocketServer() override;

    void setPersistentConnectionEnabled(bool value);

    virtual server::HttpStatistics httpStatistics() const override;

protected:
    virtual std::shared_ptr<HttpServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;

private:
    nx::network::http::server::AbstractAuthenticationManager* const m_authenticationManager;
    nx::network::http::AbstractMessageDispatcher* const m_httpMessageDispatcher;
    bool m_persistentConnectionEnabled;

    mutable QnMutex m_mutex;
    nx::network::http::server::RequestStatisticsCalculator m_statsCalculator;
};

class NX_NETWORK_API StreamConnectionHolder:
    public nx::network::server::StreamConnectionHolder<
        nx::network::http::AsyncMessagePipeline>
{
};

} // namespace nx
} // namespace network
} // namespace http
