// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "abstract_authentication_manager.h"
#include "http_message_dispatcher.h"
#include "http_server_connection.h"
#include "http_statistics.h"

namespace nx::network::http {

/**
 * Listens at a local address and accepts HTTP connections.
 *
 * Usage example:
 * <pre><code>
 * server::rest::MessageDispatcher dispatcher;
 * HttpStreamSocketServer server(httpAuthenticator, &dispatcher);
 * server.bind(nx::network::SocketAddress::anyAddress);
 * server.listen();
 * </code></pre>
 *
 * It will accept connections and route received HTTP requests go through the dispatcher.
 */
class NX_NETWORK_API HttpStreamSocketServer:
    public nx::network::server::StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>,
    public nx::network::http::server::AbstractHttpStatisticsProvider
{
    using base_type =
        nx::network::server::StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>;

public:
    using ConnectionType = HttpServerConnection;

    template<typename... Args>
    HttpStreamSocketServer(
        nx::network::http::server::AbstractAuthenticationManager* const authenticationManager,
        nx::network::http::AbstractMessageDispatcher* const httpMessageDispatcher,
        Args&&... args)
        :
        base_type(std::forward<Args>(args)...),
        m_authenticationManager(authenticationManager),
        m_httpMessageDispatcher(httpMessageDispatcher),
        m_persistentConnectionEnabled(true)
    {
    }

    virtual ~HttpStreamSocketServer() override;

    void setPersistentConnectionEnabled(bool value);

    void redirectAllRequestsTo(SocketAddress addressToRedirect);

    virtual server::HttpStatistics httpStatistics() const override;

protected:
    virtual std::shared_ptr<HttpServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;

private:
    nx::network::http::server::AbstractAuthenticationManager* const m_authenticationManager;
    nx::network::http::AbstractMessageDispatcher* const m_httpMessageDispatcher;
    bool m_persistentConnectionEnabled;

    mutable nx::Mutex m_mutex;
    nx::network::http::server::RequestStatisticsCalculator m_statsCalculator;
    std::optional<SocketAddress> m_addressToRedirect;
};

class NX_NETWORK_API StreamConnectionHolder:
    public nx::network::server::StreamConnectionHolder<
        nx::network::http::AsyncMessagePipeline>
{
};

} // namespace nx::network::http
