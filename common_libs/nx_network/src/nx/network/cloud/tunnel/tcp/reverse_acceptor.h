#pragma once

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/http_message_dispatcher.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * HTTP Server to accept NXRC/1.0 connections
 *
 * Is is safe to remove this object from any thread (destructor blocks to wait connection
 * handlers).
 */
class NX_NETWORK_API ReverseAcceptor
{
public:
    typedef utils::MoveOnlyFunc<void(
        String remoteHostName,
        std::unique_ptr<AbstractStreamSocket> socket)> ConnectHandler;

    ReverseAcceptor(ConnectHandler clientHandler);
    ~ReverseAcceptor();

    ReverseAcceptor(const ReverseAcceptor&) = delete;
    ReverseAcceptor(ReverseAcceptor&&) = delete;
    ReverseAcceptor& operator=(const ReverseAcceptor&) = delete;
    ReverseAcceptor& operator=(ReverseAcceptor&&) = delete;

    /**
     * Starts accepting connections.
     * @param aioThread is used to call handler, also removal from this thread will be nonblocking
     */
    bool start(
        String selfHostName,
        const SocketAddress& address,
        aio::AbstractAioThread* aioThread = nullptr);

    /** Stops accepting new connections (shall be called from AIO thread) */
    void pleaseStop();

    SocketAddress address() const;
    String selfHostName() const;

    // TODO: make is configurable for each client? can it be usefull?
    void setPoolSize(boost::optional<size_t> value);
    void setKeepAliveOptions(boost::optional<KeepAliveOptions> value);

private:
    void fillNxRcHeaders(nx_http::HttpHeaders* headers) const;
    void saveConnection(String name, nx_http::HttpServerConnection* connection) const;

    class NxRcHandler: public nx_http::AbstractHttpRequestHandler
    {
    public:
        NxRcHandler(const ReverseAcceptor* acceptor);

        void processRequest(
            nx_http::HttpServerConnection* const connection,
            stree::ResourceContainer authInfo,
            nx_http::Request request,
            nx_http::Response* const response,
            nx_http::RequestProcessedHandler handler) override;

    private:
        const ReverseAcceptor* m_acceptor;
    };

    String m_selfHostName;
    ConnectHandler m_connectHandler;

    mutable QnMutex m_dataMutex;
    boost::optional<size_t> m_poolSize;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;

    nx_http::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx_http::HttpStreamSocketServer> m_httpServer;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
