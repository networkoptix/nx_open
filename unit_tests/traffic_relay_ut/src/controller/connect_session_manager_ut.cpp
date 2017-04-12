#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>

#include <controller/connect_session_manager.h>
#include <model/client_session_pool.h>
#include <model/listening_peer_pool.h>
#include <settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

class ConnectSessionManager:
    public ::testing::Test
{
public:
    ConnectSessionManager():
        m_connectSessionManager(m_settings, &m_clientSessionPool, &m_listeningPeerPool),
        m_peerName(nx::utils::generateRandomName(17).toStdString())
    {
    }

protected:
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
        ASSERT_EQ(1, m_listeningPeerPool.getConnectionCountByPeerName(m_peerName));
    }

private:
    conf::Settings m_settings;
    model::ClientSessionPool m_clientSessionPool;
    model::ListeningPeerPool m_listeningPeerPool;
    controller::ConnectSessionManager m_connectSessionManager;
    std::string m_peerName;

    void onBeginListeningCompletion(
        api::ResultCode resultCode,
        api::BeginListeningResponse /*response*/,
        nx_http::ConnectionEvents connectionEvents)
    {
        ASSERT_EQ(api::ResultCode::ok, resultCode);

        auto tcpConnection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(tcpConnection->setNonBlockingMode(true));
        auto httpConnection = std::make_unique<nx_http::HttpServerConnection>(
            nullptr,
            std::move(tcpConnection),
            nullptr,
            nullptr);
        connectionEvents.onResponseHasBeenSent(httpConnection.get());
    }
};

TEST_F(ConnectSessionManager, beginListening_saves_connection_to_the_pool)
{
    whenInvokedBeginListening();
    thenTcpConnectionHasBeenSavedToThePool();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
