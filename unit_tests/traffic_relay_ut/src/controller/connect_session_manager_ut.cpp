#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>

#include <nx/network/aio/async_channel_adapter.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/stream_socket_stub.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/settings_loader.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/listening_peer_manager.h>
#include <nx/cloud/relaying/listening_peer_pool.h>
#include <nx/cloud/relay/controller/connect_session_manager.h>
#include <nx/cloud/relay/controller/traffic_relay.h>
#include <nx/cloud/relay/model/client_session_pool.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

namespace {

class RemoteRelayPeerPoolStub:
    public nx::cloud::relay::model::AbstractRemoteRelayPeerPool
{
public:
    virtual bool connectToDb() override
    {
        return true;
    }

    virtual cf::future<std::string> findRelayByDomain(
        const std::string& /*domainName*/) const override
    {
        return cf::make_ready_future(std::string());
    }

    virtual cf::future<bool> addPeer(const std::string& /*domainName*/) override
    {
        return cf::make_ready_future(true);
    }

    virtual cf::future<bool> removePeer(const std::string& /*domainName*/) override
    {
        return cf::make_ready_future(true);
    }

    virtual void setNodeId(const std::string& /*nodeId*/) {}
};

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
        using AdapterType =
            nx::network::aio::AsyncChannelAdapter<
                std::unique_ptr<AbstractStreamSocket>>;

        for (const auto& relaySession: m_relaySessions)
        {
            // TODO: #ak Get rid of conversion to AdapterType
            //   when AbstractStreamSocket inherits AbstractAsyncChannel.

            auto clientConnectionAdapter =
                dynamic_cast<AdapterType*>(relaySession.clientConnection.connection.get());
            auto serverConnectionAdapter =
                dynamic_cast<AdapterType*>(relaySession.serverConnection.connection.get());

            if (clientConnectionAdapter->adaptee().get() == clientConnection &&
                serverConnectionAdapter->adaptee().get() == serverConnection &&
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
        m_peerName(nx::utils::generateRandomName(17).toStdString())
    {
        addArg("-listeningPeer/maxPreemptiveConnectionCount", "7");
        addArg("-listeningPeer/recommendedPreemptiveConnectionCount", "4");
        addArg("-listeningPeer/internalTimerPeriod", "1ms");
        addArg("-connectingPeer/connectSessionIdleTimeout", "11s");

        m_settingsLoader.load();

        m_clientEndpoint.address = HostAddress::localhost;
        m_clientEndpoint.port = nx::utils::random::number<int>(10000, 50000);
    }

    ~ConnectSessionManager()
    {
        if (m_lastClientConnection)
            m_lastClientConnection->pleaseStopSync();

        m_connectSessionManager.reset();
    }

protected:
    std::string m_expectedSessionId;

    void addArg(const std::string& name, const std::string& value)
    {
        m_settingsLoader.addArg(name, value);
        m_settingsLoader.load();
    }

    void givenPeerConnectionCountAlreadyAtMaximum()
    {
        for (int i = 0; i < kMaxPreemptiveConnectionCount; ++i)
        {
            whenInvokedBeginListening();
            ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
        }

        ASSERT_EQ(
            (std::size_t)kMaxPreemptiveConnectionCount,
            listeningPeerPool().getConnectionCountByPeerName(m_peerName));
    }

    void givenListeningPeer()
    {
        whenInvokedBeginListening();
        ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
    }

    void givenClientSessionToListeningPeerWithIdleConnections()
    {
        addListeningPeerConnection();

        whenIssuedCreateSessionWithPredefinedId();
        thenSessionHasBeenCreated();
    }

    void whenInvokedBeginListening()
    {
        addListeningPeerConnection();
    }

    void whenFreedConnectionManager()
    {
        m_connectSessionManager.reset();
    }

    void whenRequestedConnectionToPeer()
    {
        using namespace std::placeholders;

        controller::ConnectToPeerRequestEx request;
        request.sessionId = m_expectedSessionId;
        request.clientEndpoint = m_clientEndpoint;
        connectSessionManager().connectToPeer(
            request,
            std::bind(&ConnectSessionManager::onConnectCompletion, this, _1, _2));
    }

    void whenIssuedCreateSessionWithPredefinedId()
    {
        m_expectedSessionId = QnUuid::createUuid().toSimpleString().toStdString();
        issueCreateSession(m_expectedSessionId);
    }

    void thenTcpConnectionHasBeenSavedToThePool()
    {
        ASSERT_EQ(api::ResultCode::ok, m_beginListeningResults.pop());
        ASSERT_EQ(1U, listeningPeerPool().getConnectionCountByPeerName(m_peerName));
    }

    void thenRequestHasFailed()
    {
        ASSERT_NE(api::ResultCode::ok, m_beginListeningResults.pop());
    }

    void thenConnectRequestSucceeded()
    {
        ASSERT_EQ(api::ResultCode::ok, m_connectResults.pop().code);
    }

    void thenConnectHasReportedNotFound()
    {
        ASSERT_EQ(api::ResultCode::notFound, m_connectResults.pop().code);
    }

    void thenConnectHasFailed()
    {
        ASSERT_NE(api::ResultCode::ok, m_connectResults.pop().code);
    }

    void thenProxyingHasBeenStarted()
    {
        for (;;)
        {
            if (trafficRelayStub().hasRelaySession(
                    m_lastClientConnection,
                    lastListeningPeerConnection(),
                    listeningPeerName()))
            {
                break;
            }

            std::this_thread::yield();
        }
    }

    void thenSessionHasBeenCreated()
    {
        const auto result = m_createClientSessionResults.pop();

        ASSERT_EQ(api::ResultCode::ok, result.code);
        ASSERT_FALSE(result.response.sessionId.empty());
        if (!m_expectedSessionId.empty())
        {
            ASSERT_EQ(m_expectedSessionId, result.response.sessionId);
        }
        ASSERT_EQ(kConnectSessionIdleTimeout, result.response.sessionTimeout);
        ASSERT_TRUE(boost::ends_with(
            clientSessionPool().getPeerNameBySessionId(result.response.sessionId),
            listeningPeerName()));
    }

    void thenCreateSessionReportedNotFound()
    {
        ASSERT_EQ(api::ResultCode::notFound, m_createClientSessionResults.pop().code);
    }

    void thenStartRelayingNotificationIsSentToTheListeningPeer()
    {
        const QByteArray buffer = m_lastListeningPeerConnection->read();
        nx::network::http::Message message(nx::network::http::MessageType::request);
        ASSERT_TRUE(message.request->parse(buffer));

        api::OpenTunnelNotification openTunnelNotification;
        ASSERT_TRUE(openTunnelNotification.parse(message));
        ASSERT_EQ(
            m_clientEndpoint.toString(),
            openTunnelNotification.clientEndpoint().toString());
    }

    //---------------------------------------------------------------------------------------------

    model::ClientSessionPool& clientSessionPool()
    {
        if (!m_clientSessionPool)
        {
            m_settingsLoader.load();

            m_clientSessionPool =
                std::make_unique<model::ClientSessionPool>(
                    m_settingsLoader.settings());
        }

        return *m_clientSessionPool;
    }

    TrafficRelayStub& trafficRelayStub()
    {
        return m_trafficRelayStub;
    }

    controller::ConnectSessionManager& connectSessionManager()
    {
        if (!m_connectSessionManager)
        {
            m_settingsLoader.load();

            m_connectSessionManager =
                std::make_unique<controller::ConnectSessionManager>(
                    m_settingsLoader.settings(),
                    &clientSessionPool(),
                    &listeningPeerPool(),
                    &m_remoteRelayPeerPoolStub,
                    &m_trafficRelayStub);
        }

        return *m_connectSessionManager;
    }

    relaying::ListeningPeerManager& listeningPeerManager()
    {
        if (!m_listeningPeerManager)
        {
            m_settingsLoader.load();

            m_listeningPeerManager =
                std::make_unique<relaying::ListeningPeerManager>(
                    m_settingsLoader.settings().listeningPeer(),
                    &listeningPeerPool());
        }

        return *m_listeningPeerManager;
    }

    relaying::ListeningPeerPool& listeningPeerPool()
    {
        if (!m_listeningPeerPool)
        {
            m_settingsLoader.load();

            m_listeningPeerPool =
                std::make_unique<relaying::ListeningPeerPool>(
                    m_settingsLoader.settings().listeningPeer());
        }

        return *m_listeningPeerPool;
    }

    std::string listeningPeerName() const
    {
        return m_peerName;
    }

    void setListeningPeerName(std::string listeningPeerName)
    {
        m_peerName = std::move(listeningPeerName);
    }

    void addListeningPeerConnection()
    {
        addListeningPeerConnection(m_peerName);
    }

    void addListeningPeerConnection(std::string peerName)
    {
        using namespace std::placeholders;

        api::BeginListeningRequest request;
        request.peerName = std::move(peerName);

        listeningPeerManager().beginListening(
            request,
            std::bind(&ConnectSessionManager::onBeginListeningCompletion, this, _1, _2, _3));
    }

    nx::network::test::StreamSocketStub* lastListeningPeerConnection()
    {
        return m_lastListeningPeerConnection;
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
            std::bind(&ConnectSessionManager::onCreateClientSessionCompletion,
                this, _1, _2));
    }

private:
    struct ConnectResult
    {
        api::ResultCode code = api::ResultCode::ok;
    };

    struct CreateClientSessionResult
    {
        api::ResultCode code = api::ResultCode::ok;
        api::CreateClientSessionResponse response;
    };

    std::unique_ptr<model::ClientSessionPool> m_clientSessionPool;
    std::unique_ptr<relaying::ListeningPeerPool> m_listeningPeerPool;
    TrafficRelayStub m_trafficRelayStub;
    RemoteRelayPeerPoolStub m_remoteRelayPeerPoolStub;
    std::unique_ptr<controller::ConnectSessionManager> m_connectSessionManager;
    std::unique_ptr<relaying::ListeningPeerManager> m_listeningPeerManager;
    std::string m_peerName;
    nx::utils::SyncQueue<api::ResultCode> m_beginListeningResults;
    nx::network::test::StreamSocketStub* m_lastListeningPeerConnection = nullptr;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    nx::network::test::StreamSocketStub* m_lastClientConnection = nullptr;
    nx::utils::SyncQueue<CreateClientSessionResult> m_createClientSessionResults;
    SocketAddress m_clientEndpoint;
    nx::utils::test::SettingsLoader<conf::Settings> m_settingsLoader;

    void onBeginListeningCompletion(
        api::ResultCode resultCode,
        api::BeginListeningResponse /*response*/,
        nx::network::http::ConnectionEvents connectionEvents)
    {
        if (connectionEvents.onResponseHasBeenSent)
        {
            auto tcpConnection = std::make_unique<network::test::StreamSocketStub>();
            m_lastListeningPeerConnection = tcpConnection.get();
            auto httpConnection = std::make_unique<nx::network::http::HttpServerConnection>(
                nullptr,
                std::move(tcpConnection),
                nullptr,
                nullptr);
            connectionEvents.onResponseHasBeenSent(httpConnection.get());
        }

        m_beginListeningResults.push(resultCode);
    }

    void onConnectCompletion(
        api::ResultCode resultCode,
        nx::network::http::ConnectionEvents connectionEvents)
    {
        if (connectionEvents.onResponseHasBeenSent)
        {
            auto tcpConnection = std::make_unique<network::test::StreamSocketStub>();
            tcpConnection->setForeignAddress(m_clientEndpoint);
            m_lastClientConnection = tcpConnection.get();
            auto httpConnection = std::make_unique<nx::network::http::HttpServerConnection>(
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

TEST_F(ConnectSessionManagerListeningPeer, notification_is_sent_just_before_relaying)
{
    givenClientSessionToListeningPeerWithIdleConnections();
    whenRequestedConnectionToPeer();
    thenStartRelayingNotificationIsSentToTheListeningPeer();
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
        setListeningPeerName("unknown peer");
        whenIssuedCreateSessionWithPredefinedId();
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

    void thenClientSessionsAreClosed()
    {
        for (const auto& sessionId: m_connectSessionIds)
        {
            for (;;)
            {
                controller::ConnectToPeerRequestEx request;
                request.sessionId = sessionId;
                nx::utils::promise<api::ResultCode> completed;
                connectSessionManager().connectToPeer(
                    request,
                    [&completed](api::ResultCode resultCode, nx::network::http::ConnectionEvents)
                    {
                        completed.set_value(resultCode);
                    });
                const auto resultCode = completed.get_future().get();
                if (resultCode == api::ResultCode::notFound)
                    break;
            }
        }
    }

private:
    std::vector<std::string> m_connectSessionIds;

    void registerListeningPeer()
    {
        whenInvokedBeginListening();
        thenTcpConnectionHasBeenSavedToThePool();
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
    addArg("-listeningPeer/disconnectedPeerTimeout", "1ms");
    addArg("-listeningPeer/takeIdleConnectionTimeout", "1ms");

    givenMultipleClientSessions();
    whenListeningPeerDisconnects();
    thenClientSessionsAreClosed();
}

//-------------------------------------------------------------------------------------------------

class ConnectSessionManagerConnectingByDomainName:
    public ConnectSessionManagerConnectingPeer
{
public:
    ConnectSessionManagerConnectingByDomainName()
    {
        m_domainName = utils::generateRandomName(11).toStdString();
        setListeningPeerName(m_domainName);
    }

protected:
    void givenMultipleListeningPeersWithOneConnectionEach()
    {
        m_peerCount = 7;
        m_connectionsPerPeer = 1;
        createConnections();
    }

    void givenMultipleListeningPeersWithMultipleConnectionsEach()
    {
        m_peerCount = 7;
        m_connectionsPerPeer = 7;
        createConnections();
    }

    void whenRequestedConnectionByDomainName()
    {
        whenRequestedMultipleConnectionsByDomainName();
    }

    void whenRequestedMultipleConnectionsByDomainName()
    {
        whenIssuedCreateSessionWithPredefinedId();
        thenSessionHasBeenCreated();

        for (int i = 0; i < m_connectionsPerPeer; ++i)
            whenRequestedConnectionToPeer();
    }

    void thenAllConnectionsBelongToTheSamePeer()
    {
        for (int i = 0; i < m_connectionsPerPeer; ++i)
            thenConnectRequestSucceeded();
    }

    void thenNoConnectionCouldBeRetrievedWithinSameSession()
    {
        whenRequestedConnectionToPeer();
        thenConnectHasFailed();
    }

private:
    std::string m_domainName;
    std::vector<std::string> m_listeningPeersNames;
    int m_peerCount = 0;
    int m_connectionsPerPeer = 0;

    void createConnections()
    {
        m_listeningPeersNames.resize(m_peerCount);
        for (auto& peerName: m_listeningPeersNames)
        {
            peerName = utils::generateRandomName(11).toStdString();
            for (int i = 0; i < m_connectionsPerPeer; ++i)
                addListeningPeerConnection(peerName + "." + m_domainName);
        }
    }
};

TEST_F(
    ConnectSessionManagerConnectingByDomainName,
    all_connections_within_session_go_to_the_same_peer)
{
    addArg("-listeningPeer/takeIdleConnectionTimeout", "1ms");

    givenMultipleListeningPeersWithOneConnectionEach();
    whenRequestedConnectionByDomainName();
    thenConnectRequestSucceeded();
    thenNoConnectionCouldBeRetrievedWithinSameSession();
}

TEST_F(
    ConnectSessionManagerConnectingByDomainName,
    all_connections_within_session_go_to_the_same_peer_2)
{
    givenMultipleListeningPeersWithMultipleConnectionsEach();
    whenRequestedMultipleConnectionsByDomainName();
    thenAllConnectionsBelongToTheSamePeer();
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

protected:
    void givenOngoingConnectToPeerRequest()
    {
        whenRequestedConnectionToPeer();
    }
};

TEST_F(ConnectSessionManagerConnectingPeerConnectTo, connect_to_listening_peer)
{
    whenRequestedConnectionToPeer();

    thenConnectRequestSucceeded();
    thenProxyingHasBeenStarted();
}

TEST_F(ConnectSessionManagerConnectingPeerConnectTo, waits_for_request_completion)
{
    givenOngoingConnectToPeerRequest();
    whenFreedConnectionManager();
    thenConnectRequestSucceeded();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
