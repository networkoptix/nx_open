#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/listening_peer_connection_watcher.h>

namespace nx::cloud::relaying::test {

class ListeningPeerConnectionWatcher:
    public ::testing::Test
{
    using base_type = ::testing::Test;

public:
    ~ListeningPeerConnectionWatcher()
    {
        m_serverSocket.pleaseStopSync();

        if (m_clientConnection)
            m_clientConnection->pleaseStopSync();

        if (m_acceptedConnection)
            m_acceptedConnection->pleaseStopSync();

        if (m_watcher)
            m_watcher->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(m_serverSocket.bind(nx::network::SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket.listen());

        auto clientConnection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(clientConnection->connect(
            m_serverSocket.getLocalAddress(), nx::network::kNoTimeout));
        ASSERT_TRUE(clientConnection->setNonBlockingMode(true));

        m_acceptedConnection = m_serverSocket.accept();
        ASSERT_NE(nullptr, m_acceptedConnection);

        m_clientConnection = std::make_unique<network::http::AsyncMessagePipeline>(
            std::move(clientConnection));
        m_clientConnection->setMessageHandler(
            [this](auto message) { saveNotification(std::move(message)); });
        m_clientConnection->startReadingConnection();

        initializeWatcher();
    }

    void whenStartTunnel()
    {
        // TODO: Fill m_clientInfo.

        m_watcher->startTunnel(
            m_clientInfo,
            [this](auto&&... args) { saveStartTunnelResult(std::forward<decltype(args)>(args)...); });
    }

    void thenOpenTunnelNotificationIsSent()
    {
        auto message = m_receivedNotifications.pop();
        relay::api::OpenTunnelNotification notification;
        ASSERT_TRUE(notification.parse(message));
    }

    void thenTunnelConnectionIsProvided()
    {
        auto result = m_startTunnelResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(result));
        ASSERT_NE(nullptr, std::get<1>(result));
    }

private:
    using StartTunnelResult =
        std::tuple<SystemError::ErrorCode, std::unique_ptr<network::AbstractStreamSocket>>;

    nx::network::TCPServerSocket m_serverSocket;
    std::unique_ptr<network::http::AsyncMessagePipeline> m_clientConnection;
    std::unique_ptr<network::AbstractStreamSocket> m_acceptedConnection;
    std::unique_ptr<relaying::ListeningPeerConnectionWatcher> m_watcher;
    ClientInfo m_clientInfo;
    nx::utils::SyncQueue<network::http::Message> m_receivedNotifications;
    nx::utils::SyncQueue<StartTunnelResult> m_startTunnelResults;

    void initializeWatcher()
    {
        ASSERT_TRUE(m_acceptedConnection->setNonBlockingMode(true));

        m_watcher = std::make_unique<relaying::ListeningPeerConnectionWatcher>(
            std::exchange(m_acceptedConnection, nullptr),
            "" /*peerProtocolVersion*/);

        m_watcher->start([this](auto errorCode) { saveConnectionClosure(errorCode); });
    }

    void saveConnectionClosure(SystemError::ErrorCode /*errorCode*/)
    {
        // TODO
    }

    void saveNotification(network::http::Message notificationMessage)
    {
        m_receivedNotifications.push(std::move(notificationMessage));
    }

    void saveStartTunnelResult(
        SystemError::ErrorCode resultCode,
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        m_startTunnelResults.push(std::make_tuple(resultCode, std::move(connection)));
    }
};

TEST_F(ListeningPeerConnectionWatcher, sends_open_tunnel_notification)
{
    whenStartTunnel();

    thenOpenTunnelNotificationIsSent();
    thenTunnelConnectionIsProvided();
}

} // namespace nx::cloud::relaying::test
