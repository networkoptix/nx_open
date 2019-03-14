#include <gtest/gtest.h>

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace server {
namespace test {

namespace {

/**
 * Socket non-blocking mode option is inconsistent.
 */
class BrokenStreamSocket:
    public nx::network::TCPSocket
{
public:
    virtual bool setNonBlockingMode(bool /*val*/) override
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return false;
    }

    virtual bool getNonBlockingMode(bool* val) const override
    {
        *val = true;
        return true;
    }
};

class TCPSocketWithInstanceCounter:
    public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;

public:
    TCPSocketWithInstanceCounter(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        std::atomic<int>* counter)
        :
        base_type(socket.get()),
        m_socket(std::move(socket)),
        m_counter(counter)
    {
        ++(*m_counter);
    }

    ~TCPSocketWithInstanceCounter()
    {
        --(*m_counter);
    }

private:
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    std::atomic<int>* m_counter = nullptr;
};

class TestConnection:
    public nx::network::server::BaseServerConnection<TestConnection>
{
    using base_type = nx::network::server::BaseServerConnection<TestConnection>;

public:
    TestConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(std::move(streamSocket))
    {
    }

    void bytesReceived(nx::Buffer& /*buffer*/)
    {
    }

    void readyToSendData()
    {
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ConnectionServerBaseServerConnection:
    public ::testing::Test
{
public:
    ~ConnectionServerBaseServerConnection()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
    }

protected:
    void setInactivityTimeout(std::chrono::milliseconds timeout)
    {
        m_inactivityTimeout = timeout;
    }

    void givenConnection()
    {
        auto clientConnection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(clientConnection->connect(
            m_serverSocket.getLocalAddress(), nx::network::kNoTimeout));
        m_clientConnections.push_back(std::move(clientConnection));

        auto acceptedConnection = m_serverSocket.accept();
        ASSERT_NE(nullptr, acceptedConnection);
        ASSERT_TRUE(acceptedConnection->setNonBlockingMode(true));

        initializeConnection(
            std::make_unique<TCPSocketWithInstanceCounter>(
                std::move(acceptedConnection),
                &m_socketCounter));
    }

    void givenConnectionWithBrokenSocket()
    {
        initializeConnection(std::make_unique<BrokenStreamSocket>());
    }

    void givenStartedConnection()
    {
        givenConnection();
        whenStartReadingConnection();
    }

    void whenStartReadingConnection()
    {
        nx::utils::promise<void> done;
        m_connection->post(
            [this, &done]()
            {
                m_invokingConnectionMethod = true;
                m_connection->startReadingConnection(m_inactivityTimeout);
                m_invokingConnectionMethod = false;

                done.set_value();
            });
        done.get_future().wait();
    }

    void whenChangeConnectionInactivityTimeoutTo(std::chrono::milliseconds timeout)
    {
        m_connection->executeInAioThreadSync(
            [this, timeout]() { m_connection->setInactivityTimeout(timeout); });
    }

    void whenCloseConnection()
    {
        m_connection->closeConnection(SystemError::connectionReset);
    }

    void thenFailureIsReportedDelayed()
    {
        m_connectionCloseEvents.pop();
        ASSERT_FALSE(*m_connectionClosedReportedDirectly);
    }

    void thenConnectionIsClosedByTimeout()
    {
        ASSERT_EQ(SystemError::timedOut, m_connectionCloseEvents.pop());
    }

    void thenTcpConnectionIsClosed()
    {
        // Waiting for the connection to process all its asynchronous calls.
        m_connection->executeInAioThreadSync([this]() {});

        ASSERT_EQ(0, m_socketCounter);
    }

    void andConnectionCloseEventIsReported()
    {
        ASSERT_NE(SystemError::noError, m_connectionCloseEvents.pop());
    }

private:
    std::unique_ptr<TestConnection> m_connection;
    bool m_invokingConnectionMethod = false;
    std::optional<bool> m_connectionClosedReportedDirectly;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionCloseEvents;
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    TCPServerSocket m_serverSocket;
    std::vector<std::unique_ptr<TCPSocket>> m_clientConnections;
    std::atomic<int> m_socketCounter{0};

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_serverSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket.listen());
    }

    void initializeConnection(std::unique_ptr<AbstractStreamSocket> socket)
    {
        m_connection = std::make_unique<TestConnection>(std::move(socket));
        m_connection->registerCloseHandler(
            [this](auto... args) { saveConnectionClosedEvent(args...); });
    }

    void saveConnectionClosedEvent(SystemError::ErrorCode closeReason)
    {
        m_connectionClosedReportedDirectly = m_invokingConnectionMethod;
        m_connectionCloseEvents.push(closeReason);
    }
};

TEST_F(ConnectionServerBaseServerConnection, start_read_failure_is_report_via_event_loop)
{
    givenConnectionWithBrokenSocket();
    whenStartReadingConnection();
    thenFailureIsReportedDelayed();
}

TEST_F(ConnectionServerBaseServerConnection, inactivity_timeout_works)
{
    setInactivityTimeout(std::chrono::milliseconds(1));

    givenConnection();
    whenStartReadingConnection();
    thenConnectionIsClosedByTimeout();
}

TEST_F(ConnectionServerBaseServerConnection, changing_inactivity_timeout)
{
    setInactivityTimeout(std::chrono::hours(1));

    givenStartedConnection();
    whenChangeConnectionInactivityTimeoutTo(std::chrono::milliseconds(1));
    thenConnectionIsClosedByTimeout();
}

TEST_F(ConnectionServerBaseServerConnection, connection_close_works_properly)
{
    givenStartedConnection();

    whenCloseConnection();

    thenTcpConnectionIsClosed();
    andConnectionCloseEventIsReported();
}

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
