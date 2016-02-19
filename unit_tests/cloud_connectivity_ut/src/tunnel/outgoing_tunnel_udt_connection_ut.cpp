
#include <future>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/outgoing_tunnel_udt_connection.h>
#include <nx/network/udt/udt_socket.h>
#include <utils/common/cpp14.h>


namespace nx {
namespace network {
namespace cloud {

class OutgoingTunnelUdtConnectionTest
:
    public ::testing::Test
{
public:
    OutgoingTunnelUdtConnectionTest()
    :
        m_udtServerSocket(std::make_unique<UdtStreamServerSocket>()),
        m_first(true)
    {
        m_udtServerSocket->acceptAsync(
            std::bind(
                &OutgoingTunnelUdtConnectionTest::onNewConnectionAccepted,
                this,
                std::placeholders::_1,
                std::placeholders::_2));
    }

    ~OutgoingTunnelUdtConnectionTest()
    {
        if (m_udtServerSocket)
            m_udtServerSocket->pleaseStopSync();
        if (m_controlConnection)
            m_controlConnection->pleaseStopSync();
    }

    bool start()
    {
        return
            m_udtServerSocket->bind(SocketAddress(HostAddress::localhost, 0)) &&
            m_udtServerSocket->listen();
    }

    SocketAddress serverEndpoint() const
    {
        return m_udtServerSocket->getLocalAddress();
    }

protected:
    std::unique_ptr<UdtStreamServerSocket> m_udtServerSocket;

private:
    bool m_first;
    std::unique_ptr<AbstractStreamSocket> m_controlConnection;

    void onNewConnectionAccepted(
        SystemError::ErrorCode errorCode,
        AbstractStreamSocket* socket)
    {
        if (m_first)
        {
            assert(errorCode == SystemError::noError);
            m_controlConnection.reset(socket);
            m_first = false;
        }
        else
        {
            delete socket;
        }
    }
};

struct ConnectResult
{
    SystemError::ErrorCode errorCode;
    std::unique_ptr<AbstractStreamSocket> connection;
    bool stillValid;
};

struct TestContext
{
    std::promise<ConnectResult> connectedPromise;
    std::chrono::milliseconds timeout;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
};

TEST_F(OutgoingTunnelUdtConnectionTest, common)
{
    const int connectionsToCreate = 100;

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    auto udtConnection = std::make_unique<UdtStreamSocket>();
    ASSERT_TRUE(udtConnection->connect(serverEndpoint()));

    OutgoingTunnelUdtConnection tunnelConnection(
        QnUuid::createUuid().toByteArray(),
        std::move(udtConnection));

    std::vector<std::promise<ConnectResult>> connectedPromises(connectionsToCreate);
    for (auto& connectedPromise: connectedPromises)
    {
        tunnelConnection.establishNewConnection(
            boost::none,
            SocketAttributes(),
            [&connectedPromise](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> connection,
                bool stillValid)
            {
                connectedPromise.set_value(
                    ConnectResult{ errorCode, std::move(connection), stillValid });
            });
    }

    for (std::size_t i = 0; i < connectedPromises.size(); ++i)
    {
        const auto result = connectedPromises[i].get_future().get();
        ASSERT_EQ(SystemError::noError, result.errorCode);
        ASSERT_NE(nullptr, result.connection);
        ASSERT_TRUE(result.stillValid);
    }

    tunnelConnection.pleaseStopSync();
}

TEST_F(OutgoingTunnelUdtConnectionTest, timeout)
{
    const int connectionsToCreate = 100;
    const int minTimeoutMillis = 50;
    const int maxTimeoutMillis = 2000;
    const std::chrono::milliseconds acceptableTimeoutFault(100);

    ASSERT_TRUE(start()) << SystemError::getLastOSErrorText().toStdString();

    auto udtConnection = std::make_unique<UdtStreamSocket>();
    ASSERT_TRUE(udtConnection->connect(serverEndpoint()));

    OutgoingTunnelUdtConnection tunnelConnection(
        QnUuid::createUuid().toByteArray(),
        std::move(udtConnection));

    m_udtServerSocket->pleaseStopSync();
    m_udtServerSocket.reset();
    //server socket is stopped, no connection can be accepted

    std::vector<TestContext> connectContexts(connectionsToCreate);
    for (auto& connectContext : connectContexts)
    {
        connectContext.timeout = std::chrono::milliseconds(
            minTimeoutMillis + rand() % (maxTimeoutMillis-minTimeoutMillis));
        connectContext.startTime = std::chrono::steady_clock::now();

        tunnelConnection.establishNewConnection(
            connectContext.timeout,
            SocketAttributes(),
            [&connectContext](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> connection,
                bool stillValid)
            {
                connectContext.connectedPromise.set_value(
                    ConnectResult{ errorCode, std::move(connection), stillValid });
                connectContext.endTime = std::chrono::steady_clock::now();
            });
    }

    for (std::size_t i = 0; i < connectContexts.size(); ++i)
    {
        const auto result = connectContexts[i].connectedPromise.get_future().get();
        ASSERT_EQ(SystemError::timedOut, result.errorCode);
        ASSERT_EQ(nullptr, result.connection);
        ASSERT_TRUE(result.stillValid);

        const auto connectTime =
            connectContexts[i].endTime - connectContexts[i].startTime;
        ASSERT_TRUE(connectTime > connectContexts[i].timeout - acceptableTimeoutFault);
        ASSERT_TRUE(connectTime < connectContexts[i].timeout + acceptableTimeoutFault);
    }

    tunnelConnection.pleaseStopSync();
}

TEST_F(OutgoingTunnelUdtConnectionTest, cancellation)
{
}

TEST_F(OutgoingTunnelUdtConnectionTest, controlConnectionFailure)
{
}

}   //cloud
}   //network
}   //nx
