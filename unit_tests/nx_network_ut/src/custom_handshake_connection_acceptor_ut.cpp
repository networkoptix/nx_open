#include <memory>

#include <gtest/gtest.h>

#include <nx/network/custom_handshake_connection_acceptor.h>
#include <nx/network/system_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

class CustomHandshakeConnectionAcceptor:
    public ::testing::Test
{
public:
    using Acceptor = nx::network::CustomHandshakeConnectionAcceptor<
        AbstractStreamServerSocket,
        ssl::ServerSideStreamSocket>;

    ~CustomHandshakeConnectionAcceptor()
    {
        m_acceptor->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        auto tcpServer = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(tcpServer->bind(SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(tcpServer->listen());
        ASSERT_TRUE(tcpServer->setNonBlockingMode(true));
        m_tcpServer = tcpServer.get();

        m_acceptor = std::make_unique<Acceptor>(
            std::move(tcpServer),
            [](std::unique_ptr<AbstractStreamSocket> streamSocket)
            {
                return std::make_unique<ssl::ServerSideStreamSocket>(std::move(streamSocket));
            });
        //
        m_acceptor->setHandshakeTimeout(std::chrono::hours(1));

        m_acceptor->start();
        m_acceptor->acceptAsync(
            std::bind(&CustomHandshakeConnectionAcceptor::saveAcceptedConnection, this, _1, _2));
    }

    Acceptor& acceptor()
    {
        return *m_acceptor;
    }

    void whenEstablishConnection()
    {
        m_clientConnection = std::make_unique<ssl::ClientStreamSocket>(
            std::make_unique<TCPSocket>(AF_INET));

        ASSERT_TRUE(m_clientConnection->connect(
            m_tcpServer->getLocalAddress(),
            kNoTimeout));
    }

    void whenEstablishSilentConnection()
    {
        m_clientConnection = std::make_unique<TCPSocket>(AF_INET);

        ASSERT_TRUE(m_clientConnection->connect(
            m_tcpServer->getLocalAddress(),
            kNoTimeout));
    }

    void thenConnectionIsAccepted()
    {
        ASSERT_NE(nullptr, m_acceptedConnections.pop().connection);
    }

    void thenConnectionIsClosedByServerAfterSomeTime()
    {
        std::array<char, 128> readBuf;
        ASSERT_EQ(0, m_clientConnection->recv(readBuf.data(), readBuf.size()));
    }

    void thenNoConnectionsHaveBeenAccepted()
    {
        ASSERT_TRUE(m_acceptedConnections.empty());
    }

private:
    struct AcceptResult
    {
        SystemError::ErrorCode resultCode;
        std::unique_ptr<AbstractStreamSocket> connection;

        AcceptResult() = delete;
    };

    std::unique_ptr<Acceptor> m_acceptor;
    nx::utils::SyncQueue<AcceptResult> m_acceptedConnections;
    TCPServerSocket* m_tcpServer = nullptr;
    std::unique_ptr<AbstractStreamSocket> m_clientConnection;

    void saveAcceptedConnection(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
    {
        using namespace std::placeholders;

        m_acceptedConnections.push(
            AcceptResult{systemErrorCode, std::move(streamSocket)});

        m_acceptor->acceptAsync(
            std::bind(&CustomHandshakeConnectionAcceptor::saveAcceptedConnection, this, _1, _2));
    }
};

TEST_F(CustomHandshakeConnectionAcceptor, connections_are_accepted)
{
    whenEstablishConnection();
    thenConnectionIsAccepted();
}

TEST_F(CustomHandshakeConnectionAcceptor, underlying_socket_accept_error_is_reported)
{
    // TODO
}

TEST_F(CustomHandshakeConnectionAcceptor, silent_connection_is_closed_by_server)
{
    acceptor().setHandshakeTimeout(std::chrono::milliseconds(1));

    whenEstablishSilentConnection();

    thenConnectionIsClosedByServerAfterSomeTime();
    thenNoConnectionsHaveBeenAccepted();
}

// TEST_F(CustomHandshakeConnectionAcceptor, silent_connections_do_not_block_active_connections)

} // namespace test
} // namespace network
} // namespace nx
