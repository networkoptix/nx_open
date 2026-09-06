// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/stream_socket_server.h>

#include "abstract_authentication_manager.h"
#include "http_message_dispatcher.h"
#include "http_server_connection.h"
#include "http_statistics.h"
#include "settings.h"

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
        AbstractRequestHandler* requestHandler,
        Args&&... args)
        :
        base_type(std::forward<Args>(args)...),
        m_requestHandler(requestHandler)
    {
    }

    virtual ~HttpStreamSocketServer() override;

    void setPersistentConnectionEnabled(bool value);

    /**
     * Set some HTTP headers to be included in every response.
     */
    void setExtraSuccessResponseHeaders(HttpHeaders responseHeaders);

    /** See HttpServerConnection::setMaxMessageBodySize. */
    void setMaxMessageBodySize(std::uint64_t value);

    /** See HttpServerConnection::setMaxHeadersSize. */
    void setMaxHeadersSize(std::uint64_t value);

    void redirectAllRequestsTo(SocketAddress addressToRedirect);

    virtual server::HttpStatistics httpStatistics() const override;

protected:
    virtual std::shared_ptr<HttpServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override;

private:
    nx::network::http::AbstractRequestHandler* m_requestHandler = nullptr;
    bool m_persistentConnectionEnabled = true;
    HttpHeaders m_extraSuccessResponseHeaders;
    // Safe by default even for callers that construct this server directly, bypassing
    // server::Builder/MultiEndpointServer (which would otherwise apply server::Settings
    // explicitly, including 0 to disable the cap) (ANAS-323).
    std::uint64_t m_maxMessageBodySize =
        nx::network::http::server::Settings::kDefaultMaxMessageBodySize;
    std::uint64_t m_maxHeadersSize = nx::network::http::server::Settings::kDefaultMaxHeadersSize;

    mutable nx::Mutex m_mutex;
    /** Calculates HttpStatistics aggregate statistics that live in its parent, RequestStatistics */
    nx::network::http::server::RequestStatisticsCalculator m_statisticsCalculator;
    std::optional<SocketAddress> m_addressToRedirect;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API StreamConnectionHolder:
    public nx::network::server::StreamConnectionHolder<
        nx::network::http::AsyncMessagePipeline>
{
};

} // namespace nx::network::http
