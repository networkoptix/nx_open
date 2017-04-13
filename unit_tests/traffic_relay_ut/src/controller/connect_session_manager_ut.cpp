#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <controller/connect_session_manager.h>
#include <model/client_session_pool.h>
#include <model/listening_peer_pool.h>
#include <settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

static constexpr int kMaxPreemptiveConnectionCount = 7;

class ConnectSessionManager:
    public ::testing::Test
{
public:
    ConnectSessionManager():
        m_connectSessionManager(m_settings, &m_clientSessionPool, &m_listeningPeerPool),
        m_peerName(nx::utils::generateRandomName(17).toStdString())
    {
        std::array<const char*, 1> argv{
            "--listeningPeer/maxPreemptiveConnectionCount=7"
        };

        m_settings.load(argv.size(), argv.data());
    }

protected:
    void givenPeerConnectionCountAlreadyAtMaximum()
    {
        for (int i = 0; i < kMaxPreemptiveConnectionCount; ++i)
        {
            whenInvokedBeginListening();
            ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
        }

        ASSERT_EQ(
            kMaxPreemptiveConnectionCount,
            m_listeningPeerPool.getConnectionCountByPeerName(m_peerName));
    }

    void whenInvokedBeginListening()
    {
        using namespace std::placeholders;

        api::BeginListeningRequest request;
        request.peerName = m_peerName;

        m_connectSessionManager.beginListening(
            request,
            std::bind(&ConnectSessionManager::onBeginListeningCompletion, this, _1, _2, _3));
    }

    void thenTcpConnectionHasBeenSavedToThePool()
    {
        ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
        ASSERT_EQ(1, m_listeningPeerPool.getConnectionCountByPeerName(m_peerName));
    }

    void thenRequestHasFailed()
    {
        ASSERT_NE(api::ResultCode::ok, m_beginListeningResults.pop());
    }

private:
    conf::Settings m_settings;
    model::ClientSessionPool m_clientSessionPool;
    model::ListeningPeerPool m_listeningPeerPool;
    controller::ConnectSessionManager m_connectSessionManager;
    std::string m_peerName;
    nx::utils::SyncQueue<api::ResultCode> m_beginListeningResults;

    void onBeginListeningCompletion(
        api::ResultCode resultCode,
        api::BeginListeningResponse /*response*/,
        nx_http::ConnectionEvents connectionEvents)
    {
        if (connectionEvents.onResponseHasBeenSent)
        {
            auto tcpConnection = std::make_unique<nx::network::TCPSocket>(AF_INET);
            ASSERT_TRUE(tcpConnection->setNonBlockingMode(true));
            auto httpConnection = std::make_unique<nx_http::HttpServerConnection>(
                nullptr,
                std::move(tcpConnection),
                nullptr,
                nullptr);
            connectionEvents.onResponseHasBeenSent(httpConnection.get());
        }

        m_beginListeningResults.push(resultCode);
    }
};

class ConnectSessionManagerListeningPeer:
    public ConnectSessionManager
{
};

TEST_F(ConnectSessionManagerListeningPeer, peer_connection_saved_to_the_pool)
{
    whenInvokedBeginListening();
    thenTcpConnectionHasBeenSavedToThePool();
}

TEST_F(ConnectSessionManagerListeningPeer, connection_limit)
{
    givenPeerConnectionCountAlreadyAtMaximum();
    whenInvokedBeginListening();
    thenRequestHasFailed();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
