#include <gtest/gtest.h>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/system_socket.h>

namespace nx {
namespace network {
namespace test {

namespace {

class TestConnection:
    public nx_api::BaseServerConnection<TestConnection>
{
    using base_type = nx_api::BaseServerConnection<TestConnection>;

public:
    TestConnection(
        StreamConnectionHolder<TestConnection>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(connectionManager, std::move(streamSocket))
    {
    }
};

class TestServer:
    public StreamSocketServer<TestServer, TestConnection>
{
protected:
    virtual std::shared_ptr<TestConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        return std::make_shared<TestConnection>(this, std::move(_socket));
    }
};

class ReverseConnection:
    public BufferedStreamSocket,
    public AbstractAcceptableReverseConnection
{
public:
    ReverseConnection(SocketAddress remotePeerEndpoint):
        BufferedStreamSocket(std::make_unique<TCPSocket>(AF_INET)),
        m_remotePeerEndpoint(std::move(remotePeerEndpoint))
    {
    }

    virtual void connectAsync(
        ReverseConnectionCompletionHandler handler) override
    {
        BufferedStreamSocket::connectAsync(m_remotePeerEndpoint, std::move(handler));
    }

    virtual void waitForConnectionToBeReadyAsync(
        ReverseConnectionCompletionHandler handler) override
    {
        catchRecvEvent(std::move(handler));
    }

private:
    const SocketAddress m_remotePeerEndpoint;
};

} // namespace

class ReverseConnectionAcceptor:
    public ::testing::Test
{
public:
    ReverseConnectionAcceptor():
        m_acceptor(std::bind(&ReverseConnectionAcceptor::createReverseConnection, this))
    {
    }

protected:
    void givenReadyConnection()
    {
        // TODO
    }

    void whenInvokedAcceptAsync()
    {
        // TODO
    }

    void whenConnectionBecameReady()
    {
        // TODO
    }

    void thenConnectionHasBeenProvided()
    {
        // TODO
    }

private:
    network::ReverseConnectionAcceptor<ReverseConnection> m_acceptor;
    
    std::unique_ptr<ReverseConnection> createReverseConnection()
    {
        return std::make_unique<ReverseConnection>(SocketAddress());
    }
};

TEST_F(ReverseConnectionAcceptor, ready_connection_is_returned_by_listening_acceptAsync)
{
    whenInvokedAcceptAsync();
    whenConnectionBecameReady();
    thenConnectionHasBeenProvided();
}

TEST_F(ReverseConnectionAcceptor, ready_connection_is_peeked_by_acceptAsync)
{
    givenReadyConnection();
    whenInvokedAcceptAsync();
    thenConnectionHasBeenProvided();
}

//TEST_F(ReverseConnectionAcceptor, ready_connection_is_peeked_by_getNextConnectionIfAny)
//TEST_F(ReverseConnectionAcceptor, preemptive_connection_count_is_followed)
//TEST_F(ReverseConnectionAcceptor, forwards_connection_error_to_the_caller)
//TEST_F(ReverseConnectionAcceptor, stops_listening_connections_on_queue_overflow)

} // namespace test
} // namespace network
} // namespace nx
