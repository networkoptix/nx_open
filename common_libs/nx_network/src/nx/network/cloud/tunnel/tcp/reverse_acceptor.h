#pragma once

#include <nx/network/aio/basic_pollable.h>
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
class NX_NETWORK_API ReverseAcceptor:
    public aio::BasicPollable
{
    using Parent = aio::BasicPollable;

public:
    typedef utils::MoveOnlyFunc<void(
        String remoteHostName,
        std::unique_ptr<AbstractStreamSocket> socket)> ConnectHandler;

    ReverseAcceptor(ConnectHandler clientHandler);
    virtual ~ReverseAcceptor() override;

    ReverseAcceptor(const ReverseAcceptor&) = delete;
    ReverseAcceptor(ReverseAcceptor&&) = delete;
    ReverseAcceptor& operator=(const ReverseAcceptor&) = delete;
    ReverseAcceptor& operator=(ReverseAcceptor&&) = delete;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Starts accepting connections.
     * @param aioThread is used to call handler, also removal from this thread will be nonblocking
     */
    bool start(
        String selfHostName,
        const SocketAddress& address,
        aio::AbstractAioThread* aioThread = nullptr);

    SocketAddress address() const;
    String selfHostName() const;

    // TODO: make is configurable for each client? can it be usefull?
    void setPoolSize(boost::optional<size_t> value);
    void setKeepAliveOptions(boost::optional<KeepAliveOptions> value);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void fillNxRcHeaders(nx::network::http::HttpHeaders* headers) const;
    void saveConnection(String name, nx::network::http::HttpServerConnection* connection);

    class NxRcHandler: public nx::network::http::AbstractHttpRequestHandler
    {
    public:
        NxRcHandler(ReverseAcceptor* acceptor);

        void processRequest(
            nx::network::http::HttpServerConnection* const connection,
            nx::utils::stree::ResourceContainer authInfo,
            nx::network::http::Request request,
            nx::network::http::Response* const response,
            nx::network::http::RequestProcessedHandler handler) override;

    private:
        ReverseAcceptor* m_acceptor;
    };

    String m_selfHostName;
    ConnectHandler m_connectHandler;

    mutable QnMutex m_dataMutex;
    boost::optional<size_t> m_poolSize;
    boost::optional<KeepAliveOptions> m_keepAliveOptions;

    nx::network::http::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx::network::http::HttpStreamSocketServer> m_httpServer;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
