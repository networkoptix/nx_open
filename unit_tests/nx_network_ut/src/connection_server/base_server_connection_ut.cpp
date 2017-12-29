#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace server {
namespace test {

namespace {

class TestStreamSocket:
    public nx::network::TCPSocket
{
public:
    virtual bool setNonBlockingMode(bool /*val*/) override
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return false;
    }
};

class TestConnection:
    public nx::network::server::BaseServerConnection<TestConnection>
{
    using base_type = nx::network::server::BaseServerConnection<TestConnection>;

public:
    TestConnection(
        StreamConnectionHolder<TestConnection>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(connectionManager, std::move(streamSocket))
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
    public ::testing::Test,
    public nx::network::server::StreamConnectionHolder<TestConnection>
{
protected:
    void setInactivityTimeout(std::chrono::milliseconds timeout)
    {
        m_inactivityTimeout = timeout;
    }

    void givenConnection()
    {
        auto clientConnection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(clientConnection->connect(m_serverSocket.getLocalAddress(), nx::network::kNoTimeout));
        m_clientConnections.push_back(std::move(clientConnection));

        std::unique_ptr<AbstractStreamSocket> acceptedConnection(m_serverSocket.accept());
        ASSERT_NE(nullptr, acceptedConnection);
        ASSERT_TRUE(acceptedConnection->setNonBlockingMode(true));

        m_connection = std::make_unique<TestConnection>(
            this,
            std::move(acceptedConnection));
    }

    void givenConnectionWithBrokenSocket()
    {
        m_connection = std::make_unique<TestConnection>(
            this,
            std::make_unique<TestStreamSocket>());
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

    void thenFailureIsReportedDelayed()
    {
        m_connectionCloseEvents.pop();
        ASSERT_FALSE(*m_connectionClosedReportedDirectly);
    }

    void thenConnectionIsClosedByTimeout()
    {
        ASSERT_EQ(SystemError::timedOut, m_connectionCloseEvents.pop());
    }

private:
    std::unique_ptr<TestConnection> m_connection;
    bool m_invokingConnectionMethod = false;
    boost::optional<bool> m_connectionClosedReportedDirectly;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionCloseEvents;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    TCPServerSocket m_serverSocket;
    std::vector<std::unique_ptr<TCPSocket>> m_clientConnections;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_serverSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket.listen());
    }

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override
    {
        ASSERT_EQ(m_connection.get(), connection);
        m_connection.reset();
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

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
