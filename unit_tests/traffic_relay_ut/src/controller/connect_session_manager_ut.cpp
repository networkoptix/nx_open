#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <controller/connect_session_manager.h>
#include <controller/traffic_relay.h>
#include <model/client_session_pool.h>
#include <model/listening_peer_pool.h>
#include <settings.h>

#include "../stream_socket_stub.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

namespace {

class TrafficRelayStub:
    public controller::AbstractTrafficRelay
{
public:
    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) override
    {
        m_relaySessions.push_back(
            {std::move(clientConnection), std::move(serverConnection)});
    }

    bool hasRelaySession(
        AbstractStreamSocket* clientConnection,
        AbstractStreamSocket* serverConnection,
        const std::string& listeningPeerName) const
    {
        for (const auto& relaySession: m_relaySessions)
        {
            if (relaySession.clientConnection.connection.get() == clientConnection &&
                relaySession.serverConnection.connection.get() == serverConnection &&
                relaySession.serverConnection.peerId == listeningPeerName)
            {
                return true;
            }
        }

        return false;
    }

private:
    struct RelaySessionContext
    {
        RelayConnectionData clientConnection;
        RelayConnectionData serverConnection;
    };

    std::deque<RelaySessionContext> m_relaySessions;
};

} // namespace

static constexpr int kMaxPreemptiveConnectionCount = 7;
static constexpr int kRecommendedPreemptiveConnectionCount = 4;
static constexpr std::chrono::seconds kConnectSessionIdleTimeout = 
    std::chrono::seconds(11);

class ConnectSessionManager:
    public ::testing::Test
{
public:
    ConnectSessionManager():
        m_clientSessionPool(m_settings),
        m_connectSessionManager(
            m_settings,
            &m_clientSessionPool,
            &m_listeningPeerPool,
            &m_trafficRelayStub),
        m_peerName(nx::utils::generateRandomName(17).toStdString())
    {
        std::array<const char*, 3> argv{
            "--listeningPeer/maxPreemptiveConnectionCount=7",
            "--listeningPeer/recommendedPreemptiveConnectionCount=4",
            "--connectingPeer/connectSessionIdleTimeout=11s"
        };

        m_settings.load(static_cast<int>(argv.size()), argv.data());
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
            (std::size_t)kMaxPreemptiveConnectionCount,
            m_listeningPeerPool.getConnectionCountByPeerName(m_peerName));
    }

    void whenInvokedBeginListening()
    {
        using namespace std::placeholders;

        addListeningPeerConnection();
    }

    void thenTcpConnectionHasBeenSavedToThePool()
    {
        ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
        ASSERT_EQ(1U, m_listeningPeerPool.getConnectionCountByPeerName(m_peerName));
    }

    void thenRequestHasFailed()
    {
        ASSERT_NE(api::ResultCode::ok, m_beginListeningResults.pop());
    }

    model::ClientSessionPool& clientSessionPool()
    {
        return m_clientSessionPool;
    }

    TrafficRelayStub& trafficRelayStub()
    {
        return m_trafficRelayStub;
    }

    controller::ConnectSessionManager& connectSessionManager()
    {
        return m_connectSessionManager;
    }

    std::string listeningPeerName() const
    {
        return m_peerName;
    }

    void addListeningPeerConnection()
    {
        using namespace std::placeholders;

        api::BeginListeningRequest request;
        request.peerName = m_peerName;

        m_connectSessionManager.beginListening(
            request,
            std::bind(&ConnectSessionManager::onBeginListeningCompletion, this, _1, _2, _3));
    }

    StreamSocketStub* lastListeningPeerConnection()
    {
        return m_lastListeningPeerConnection;
    }

private:
    conf::Settings m_settings;
    model::ClientSessionPool m_clientSessionPool;
    model::ListeningPeerPool m_listeningPeerPool;
    TrafficRelayStub m_trafficRelayStub;
    controller::ConnectSessionManager m_connectSessionManager;
    std::string m_peerName;
    nx::utils::SyncQueue<api::ResultCode> m_beginListeningResults;
    StreamSocketStub* m_lastListeningPeerConnection = nullptr;

    void onBeginListeningCompletion(
        api::ResultCode resultCode,
        api::BeginListeningResponse /*response*/,
        nx_http::ConnectionEvents connectionEvents)
    {
        if (connectionEvents.onResponseHasBeenSent)
        {
            auto tcpConnection = std::make_unique<StreamSocketStub>();
            m_lastListeningPeerConnection = tcpConnection.get();
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
        m_listeningPeerName = listeningPeerName();
    }

protected:
    void givenClientSessionToListeningPeerWithIdleConnections()
    {
        addListeningPeerConnection();
        
        whenIssuedCreateSessionWithPredefinedId();
        thenSessionHasBeenCreated();
    }

    void givenMultipleClientSessions()
    {
        m_connectSessionIds.resize(3);
        for (auto& sessionId: m_connectSessionIds)
        {
            sessionId = nx::utils::generateRandomName(11).toStdString();
            issueCreateSession(sessionId);
            thenSessionHasBeenCreated();
        }
    }

    void whenIssuedCreateSessionWithoutId()
    {
        issueCreateSession(std::string());
    }
    
    void whenIssuedCreateSessionWithPredefinedId()
    {
        m_expectedSessionId = QnUuid::createUuid().toSimpleString().toStdString();
        issueCreateSession(m_expectedSessionId);
    }

    void whenIssuedCreateSessionWithAlreadyUsedId()
    {
        whenIssuedCreateSessionWithPredefinedId();
        thenSessionHasBeenCreated();

        std::string sessionId;
        m_expectedSessionId.swap(sessionId);
        issueCreateSession(sessionId);
    }

    void whenIssuedCreateSessionWithUnknownPeerName()
    {
        m_listeningPeerName = "unknown peer";
        whenIssuedCreateSessionWithPredefinedId();
    }

    void whenRequestedConnectionToPeer()
    {
        using namespace std::placeholders;

        api::ConnectToPeerRequest request;
        request.sessionId = m_expectedSessionId;
        connectSessionManager().connectToPeer(
            request,
            std::bind(&ConnectSessionManagerConnectingPeer::onConnectCompletion, this, _1, _2));
    }

    void whenIssuedConnectUsingUnknownSessionId()
    {
        m_expectedSessionId = "abra kadabra";
        whenRequestedConnectionToPeer();
    }

    void whenListeningPeerDisconnects()
    {
        lastListeningPeerConnection()->setConnectionToClosedState();
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

    void thenCreateSessionReportedNotFound()
    {
        ASSERT_EQ(api::ResultCode::notFound, m_createClientSessionResults.pop().code);
    }

    void thenConnectRequestSucceeded()
    {
        ASSERT_EQ(api::ResultCode::ok, m_connectResults.pop().code);
    }

    void thenProxyingHasBeenStarted()
    {
        ASSERT_TRUE(
            trafficRelayStub().hasRelaySession(
                m_lastClientConnection,
                lastListeningPeerConnection(),
                m_listeningPeerName));
    }

    void thenConnectHasReportedNotFound()
    {
        ASSERT_EQ(api::ResultCode::notFound, m_connectResults.pop().code);
    }

    void thenClientSessionsAreClosed()
    {
        for (const auto& sessionId: m_connectSessionIds)
        {
            for (;;)
            {
                api::ConnectToPeerRequest request;
                request.sessionId = sessionId;
                nx::utils::promise<api::ResultCode> completed;
                connectSessionManager().connectToPeer(
                    request,
                    [&completed](api::ResultCode resultCode, nx_http::ConnectionEvents)
                    {
                        completed.set_value(resultCode);
                    });
                const auto resultCode = completed.get_future().get();
                if (resultCode == api::ResultCode::ok)
                    continue;
                ASSERT_EQ(api::ResultCode::notFound, resultCode);
                break;
            }
        }
    }

private:
    struct CreateClientSessionResult
    {
        api::ResultCode code = api::ResultCode::ok;
        api::CreateClientSessionResponse response;
    };

    struct ConnectResult
    {
        api::ResultCode code = api::ResultCode::ok;
    };

    nx::utils::SyncQueue<CreateClientSessionResult> m_createClientSessionResults;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    std::string m_expectedSessionId;
    std::string m_listeningPeerName;
    StreamSocketStub* m_lastClientConnection = nullptr;
    std::vector<std::string> m_connectSessionIds;

    void registerListeningPeer()
    {
        whenInvokedBeginListening();
        thenTcpConnectionHasBeenSavedToThePool();
    }

    void issueCreateSession(const std::string& sessionId)
    {
        using namespace std::placeholders;

        api::CreateClientSessionRequest request;
        request.targetPeerName = m_listeningPeerName;
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

    void onConnectCompletion(
        api::ResultCode resultCode,
        nx_http::ConnectionEvents connectionEvents)
    {
        if (connectionEvents.onResponseHasBeenSent)
        {
            auto tcpConnection = std::make_unique<StreamSocketStub>();
            m_lastClientConnection = tcpConnection.get();
            auto httpConnection = std::make_unique<nx_http::HttpServerConnection>(
                nullptr,
                std::move(tcpConnection),
                nullptr,
                nullptr);
            connectionEvents.onResponseHasBeenSent(httpConnection.get());
        }

        ConnectResult result;
        result.code = resultCode;
        m_connectResults.push(result);
    }
};

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_auto_generate_id)
{
    whenIssuedCreateSessionWithoutId();
    thenSessionHasBeenCreated();
}

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_desired_session_id)
{
    whenIssuedCreateSessionWithPredefinedId();
    thenSessionHasBeenCreated();
}

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_id_is_already_used)
{
    whenIssuedCreateSessionWithAlreadyUsedId();
    thenSessionHasBeenCreated();
}

TEST_F(ConnectSessionManagerConnectingPeer, create_client_session_to_unknown_peer)
{
    whenIssuedCreateSessionWithUnknownPeerName();
    thenCreateSessionReportedNotFound();
}

TEST_F(ConnectSessionManagerConnectingPeer, connect_with_unknown_session_id)
{
    whenIssuedConnectUsingUnknownSessionId();
    thenConnectHasReportedNotFound();
}

TEST_F(
    ConnectSessionManagerConnectingPeer,
    all_client_sessions_are_closed_when_listening_peer_disappears)
{
    givenMultipleClientSessions();
    whenListeningPeerDisconnects();
    thenClientSessionsAreClosed();
}

//-------------------------------------------------------------------------------------------------

class ConnectSessionManagerConnectingPeerConnectTo:
    public ConnectSessionManagerConnectingPeer
{
public:
    ConnectSessionManagerConnectingPeerConnectTo()
    {
        whenIssuedCreateSessionWithPredefinedId();
        thenSessionHasBeenCreated();
    }
};

TEST_F(ConnectSessionManagerConnectingPeerConnectTo, connect_to_listening_peer)
{
    whenRequestedConnectionToPeer();

    thenConnectRequestSucceeded();
    thenProxyingHasBeenStarted();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
