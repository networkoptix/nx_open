#include <memory>

#include <gtest/gtest.h>

#include <nx/network/custom_handshake_connection_acceptor.h>
#include <nx/network/system_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

namespace {

class FailingTcpServerSocket:
    public TCPServerSocket
{
public:
    FailingTcpServerSocket(SystemError::ErrorCode systemErrorCode):
        m_systemErrorCode(systemErrorCode)
    {
    }

    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                handler(m_systemErrorCode, nullptr);
            });
    }

private:
    const SystemError::ErrorCode m_systemErrorCode;
};

} // namespace

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
        auto tcpServer = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(tcpServer->bind(SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(tcpServer->listen());
        ASSERT_TRUE(tcpServer->setNonBlockingMode(true));
        m_tcpServer = tcpServer.get();

        initializeAcceptor(std::move(tcpServer));
    }

    Acceptor& acceptor()
    {
        return *m_acceptor;
    }

    void givenManySilentConnectionCountGreaterThanListenQueue()
    {
        openSilentConnections(
            m_acceptor->readyConnectionQueueSize()*2);
    }

    void whenEstablishValidConnection()
    {
        m_clientConnection = std::make_unique<ssl::ClientStreamSocket>(
            std::make_unique<TCPSocket>(AF_INET));

        ASSERT_TRUE(m_clientConnection->connect(
            m_tcpServer->getLocalAddress(),
            kNoTimeout));
    }

    void whenEstablishSilentConnection()
    {
        openSilentConnections(1);
        m_clientConnection = std::move(m_silentConnections.front());
        ASSERT_TRUE(m_clientConnection->setNonBlockingMode(false));
        m_silentConnections.pop_front();
    }

    void whenRawServerConnectionReportsError()
    {
        m_acceptor->pleaseStopSync();
        m_acceptor.reset();

        initializeAcceptor(
            std::make_unique<FailingTcpServerSocket>(SystemError::ioError));
    }

    void whenRawServerConnectionReportsTimedOut()
    {
        m_acceptor->pleaseStopSync();
        m_acceptor.reset();

        initializeAcceptor(
            std::make_unique<FailingTcpServerSocket>(SystemError::timedOut));
    }

    void thenConnectionIsAccepted()
    {
        ASSERT_NE(nullptr, m_acceptedConnections.pop().connection);
    }

    void thenConnectionIsClosedByServerAfterSomeTime()
    {
        std::array<char, 128> readBuf;
        ASSERT_EQ(0, m_clientConnection->recv(readBuf.data(), readBuf.size()))
            << SystemError::getLastOSErrorText().toStdString();
    }

    void thenNoConnectionsHaveBeenAccepted()
    {
        ASSERT_TRUE(m_acceptedConnections.empty());
    }

    void thenCorrespondingErrorIsReported()
    {
        ASSERT_NE(SystemError::noError, m_acceptedConnections.pop().resultCode);
    }

    void thenErrorIsNotReported()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    std::deque<std::unique_ptr<AbstractStreamSocket>> m_silentConnections;

    void initializeAcceptor(
        std::unique_ptr<AbstractStreamServerSocket> rawConnectionSocket)
    {
        using namespace std::placeholders;

        m_acceptor = std::make_unique<Acceptor>(
            std::move(rawConnectionSocket),
            [](std::unique_ptr<AbstractStreamSocket> streamSocket)
            {
                return std::make_unique<ssl::ServerSideStreamSocket>(std::move(streamSocket));
            });
        // Using very large timeout by default to make test reliable to various delays.
        m_acceptor->setHandshakeTimeout(std::chrono::hours(1));

        m_acceptor->start();
        m_acceptor->acceptAsync(
            std::bind(&CustomHandshakeConnectionAcceptor::saveAcceptedConnection, this, _1, _2));
    }

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

    void openSilentConnections(int count)
    {
        m_silentConnections.resize(count);
        nx::utils::SyncQueue<SystemError::ErrorCode> connectResults;

        for (int i = 0; i < count; ++i)
        {
            m_silentConnections[i] = std::make_unique<TCPSocket>(AF_INET);
            ASSERT_TRUE(m_silentConnections[i]->setNonBlockingMode(true));
            m_silentConnections[i]->connectAsync(
                m_tcpServer->getLocalAddress(),
                [&connectResults](SystemError::ErrorCode resultCode)
                {
                    connectResults.push(resultCode);
                });
        }

        for (int i = 0; i < count; ++i)
        {
            ASSERT_EQ(SystemError::noError, connectResults.pop());
        }
    }
};

TEST_F(CustomHandshakeConnectionAcceptor, connections_are_accepted)
{
    whenEstablishValidConnection();
    thenConnectionIsAccepted();
}

TEST_F(CustomHandshakeConnectionAcceptor, underlying_socket_accept_error_is_forwarded)
{
    whenRawServerConnectionReportsError();
    thenCorrespondingErrorIsReported();
}

TEST_F(CustomHandshakeConnectionAcceptor, underlying_socket_accept_timeout_is_not_forwarded)
{
    whenRawServerConnectionReportsTimedOut();
    thenErrorIsNotReported();
}

TEST_F(CustomHandshakeConnectionAcceptor, silent_connection_is_closed_by_server)
{
    acceptor().setHandshakeTimeout(std::chrono::milliseconds(1));

    whenEstablishSilentConnection();

    thenConnectionIsClosedByServerAfterSomeTime();
    thenNoConnectionsHaveBeenAccepted();
}

TEST_F(CustomHandshakeConnectionAcceptor, silent_connections_do_not_block_active_connections)
{
    givenManySilentConnectionCountGreaterThanListenQueue();
    whenEstablishValidConnection();
    thenConnectionIsAccepted();
}

} // namespace test
} // namespace network
} // namespace nx
