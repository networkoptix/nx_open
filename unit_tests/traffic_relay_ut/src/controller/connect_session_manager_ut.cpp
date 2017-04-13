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
static constexpr int kRecommendedPreemptiveConnectionCount = 4;
static constexpr std::chrono::seconds kConnectSessionIdleTimeout = 
    std::chrono::seconds(11);

class ConnectSessionManager:
    public ::testing::Test
{
public:
    ConnectSessionManager():
        m_connectSessionManager(m_settings, &m_clientSessionPool, &m_listeningPeerPool),
        m_peerName(nx::utils::generateRandomName(17).toStdString())
    {
        std::array<const char*, 3> argv{
            "--listeningPeer/maxPreemptiveConnectionCount=7",
            "--listeningPeer/recommendedPreemptiveConnectionCount=4",
            "--connectingPeer/connectSessionIdleTimeout=11s"
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

    model::ClientSessionPool& clientSessionPool()
    {
        return m_clientSessionPool;
    }

    controller::ConnectSessionManager& connectSessionManager()
    {
        return m_connectSessionManager;
    }

    std::string listeningPeerName() const
    {
        return m_peerName;
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

//-------------------------------------------------------------------------------------------------
// Listening peer tests.

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

//-------------------------------------------------------------------------------------------------
// Connecting peer tests.

class ConnectSessionManagerConnectingPeer:
    public ConnectSessionManager
{
public:
    ConnectSessionManagerConnectingPeer()
    {
        registerListeningPeer();
    }

protected:
    void whenIssuedCreateSessionWithoutId()
    {
        issueCreateSession(std::string());
    }
    
    void whenIssuedCreateSessionWithoutPredefinedId()
    {
        m_expectedSessionId = QnUuid::createUuid().toStdString();
        issueCreateSession(m_expectedSessionId);
    }

    void thenSessionHasBeenCreated()
    {
        const auto result = m_createClientSessionResults.pop();

        ASSERT_EQ(api::ResultCode::ok, result.code);
        ASSERT_FALSE(result.response.sessionId.empty());
        if (!m_expectedSessionId.empty())
            ASSERT_EQ(m_expectedSessionId, result.response.sessionId);
        ASSERT_EQ(kConnectSessionIdleTimeout, result.response.sessionTimeout);
        ASSERT_EQ(
            listeningPeerName(),
            clientSessionPool().getPeerNameBySessionId(result.response.sessionId));
    }

private:
    struct CreateClientSessionResult
    {
        api::ResultCode code = api::ResultCode::ok;
        api::CreateClientSessionResponse response;
    };

    nx::utils::SyncQueue<CreateClientSessionResult> m_createClientSessionResults;
    std::string m_expectedSessionId;

    void registerListeningPeer()
    {
        whenInvokedBeginListening();
        thenTcpConnectionHasBeenSavedToThePool();
    }

    void issueCreateSession(const std::string& sessionId)
    {
        using namespace std::placeholders;

        api::CreateClientSessionRequest request;
        request.targetPeerName = listeningPeerName();
        if (!sessionId.empty())
            request.desiredSessionId = sessionId;
        connectSessionManager().createClientSession(
            std::move(request),
            std::bind(&ConnectSessionManagerConnectingPeer::onCreateClientSessionCompletion,
                this, _1, _2));
    }

    void onCreateClientSessionCompletion(
        api::ResultCode resultCode,
        api::CreateClientSessionResponse response)
    {
        CreateClientSessionResult result;
        result.code = resultCode;
        result.response = std::move(response);
        m_createClientSessionResults.push(std::move(result));
    }
};

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_auto_generate_id)
{
    whenIssuedCreateSessionWithoutId();
    thenSessionHasBeenCreated();
}

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_desired_session_id)
{
    whenIssuedCreateSessionWithoutPredefinedId();
    thenSessionHasBeenCreated();
}

//TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_id_is_already_used)
//TEST_F(ConnectSessionManagerConnectingPeer, client_session_expiration)

//TEST_F(ConnectSessionManagerConnectingPeer, connect_to_listening_peer)
//TEST_F(ConnectSessionManagerConnectingPeer, connect_to_unknown_peer)
//TEST_F(ConnectSessionManagerConnectingPeer, connect_with_unknown_session_id)
//TEST_F(ConnectSessionManagerConnectingPeer, connect_to_listening_peer_with_no_idle_connections)

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
