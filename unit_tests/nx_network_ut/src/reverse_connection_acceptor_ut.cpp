#include <set>
#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

namespace {

class TestConnection:
    public nx::network::server::BaseServerConnection<TestConnection>
{
    using base_type = nx::network::server::BaseServerConnection<TestConnection>;

public:
    TestConnection(
        nx::network::server::StreamConnectionHolder<TestConnection>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(connectionManager, std::move(streamSocket))
    {
    }

    void readyToSendData()
    {
        // TODO
    }

    void bytesReceived(const nx::Buffer& /*buf*/)
    {
        // TODO
    }
};

//-------------------------------------------------------------------------------------------------

class TestServer:
    public nx::network::server::StreamSocketServer<TestServer, TestConnection>
{
    using base_type = server::StreamSocketServer<TestServer, TestConnection>;

public:
    TestServer():
        base_type(false, NatTraversalSupport::disabled)
    {
        m_randomData.resize(100);
    }

    void sendAnythingThroughAnyConnection()
    {
        QnMutexLocker lock(&m_mutex);
        (*m_connections.begin())->sendBufAsync(m_randomData);
    }

    std::size_t connectionCount() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_connections.size();
    }

protected:
    virtual std::shared_ptr<TestConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
    {
        auto connection = std::make_shared<TestConnection>(this, std::move(_socket));
        {
            QnMutexLocker lock(&m_mutex);
            m_connections.erase(connection.get());
        }
        return connection;
    }

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        TestConnection* connection) override
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_connections.erase(connection);
        }
        base_type::closeConnection(closeReason, connection);
    }

private:
    mutable QnMutex m_mutex;
    std::set<TestConnection*> m_connections;
    nx::Buffer m_randomData;
};

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

class ReverseConnectionAcceptor:
    public ::testing::Test
{
public:
    ReverseConnectionAcceptor():
        m_acceptor(std::bind(&ReverseConnectionAcceptor::createReverseConnection, this))
    {
    }

    ~ReverseConnectionAcceptor()
    {
        m_server.pleaseStopSync();
    }

protected:
    void givenReadyConnection()
    {
        whenConnectionBecameReady();
    }

    void whenInvokedAcceptAsync()
    {
        using namespace std::placeholders;

        m_acceptor.acceptAsync(
            std::bind(&ReverseConnectionAcceptor::onAccepted, this, _1, _2));
    }

    void whenConnectionBecameReady()
    {
        while (m_server.connectionCount() == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        m_server.sendAnythingThroughAnyConnection();
    }

    void thenConnectionHasBeenAccepted()
    {
        auto acceptResult = m_acceptedConnections.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(acceptResult));
        ASSERT_NE(nullptr, std::get<1>(acceptResult));
    }

private:
    using AcceptResult = 
        std::tuple<SystemError::ErrorCode, std::unique_ptr<ReverseConnection>>;

    nx::utils::SyncQueue<AcceptResult> m_acceptedConnections;
    network::ReverseConnectionAcceptor<ReverseConnection> m_acceptor;
    TestServer m_server;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_server.bind(SocketAddress::anyPrivateAddress) && m_server.listen());
        m_acceptor.start();
    }
    
    std::unique_ptr<ReverseConnection> createReverseConnection()
    {
        return std::make_unique<ReverseConnection>(m_server.address());
    }

    void onAccepted(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<ReverseConnection> connection)
    {
        m_acceptedConnections.push(
            std::make_tuple(sysErrorCode, std::move(connection)));
    }
};

TEST_F(ReverseConnectionAcceptor, ready_connection_is_returned_by_listening_acceptAsync)
{
    whenInvokedAcceptAsync();
    whenConnectionBecameReady();
    thenConnectionHasBeenAccepted();
}

TEST_F(ReverseConnectionAcceptor, ready_connection_is_peeked_by_acceptAsync)
{
    givenReadyConnection();
    whenInvokedAcceptAsync();
    thenConnectionHasBeenAccepted();
}

//TEST_F(ReverseConnectionAcceptor, ready_connection_is_peeked_by_getNextConnectionIfAny)
//TEST_F(ReverseConnectionAcceptor, preemptive_connection_count_is_followed)
//TEST_F(ReverseConnectionAcceptor, forwards_connection_error_to_the_caller)
//TEST_F(ReverseConnectionAcceptor, stops_listening_connections_on_queue_overflow)

} // namespace test
} // namespace network
} // namespace nx
