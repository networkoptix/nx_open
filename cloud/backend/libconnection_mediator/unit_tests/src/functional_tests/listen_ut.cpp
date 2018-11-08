#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/cloud/mediator_server_connections.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/listening_peer_pool.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/relay/relay_cluster_client_factory.h>
#include <nx/cloud/mediator/test_support/mediaserver_emulator.h>

#include "mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

namespace HttpStatusCode = nx::network::http::StatusCode;

class ListeningPeer:
    public MediatorFunctionalTest,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    ListeningPeer():
        m_serverId(QnUuid::createUuid().toSimpleByteArray())
    {
    }

    ~ListeningPeer()
    {
        for (auto& connection: m_serverConnections)
            connection->pleaseStopSync();

        m_mediatorClient.reset();
    }

    virtual std::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = m_system.id;
        systemCredentials.serverId = m_serverId;
        systemCredentials.key = m_system.authKey;
        return systemCredentials;
    }

protected:
    // TODO: #ak Get rid of this method. Logically, it is a duplicate of a givenListeningServer.
    void givenListeningMediaServer()
    {
        m_mediaServerEmulator = addRandomServer(system(), boost::none);
        ASSERT_NE(nullptr, m_mediaServerEmulator);
        ASSERT_EQ(
            nx::hpm::api::ResultCode::ok,
            m_mediaServerEmulator->listen().first);
    }

    void givenListeningServer()
    {
        whenServerEstablishesNewConnection();
    }

    void whenStopServer()
    {
        m_mediaServerEmulator.reset();
    }

    void whenServerEstablishesNewConnection()
    {
        establishListeningConnectionToMediatorAsync();
        assertConnectionToMediatorHasBeenEstablished();
    }

    void whenCloseConnectionToMediator()
    {
        m_serverConnections.back()->pleaseStopSync();
        m_serverConnections.pop_back();
    }

    void whenConnectToPeerOverHttp(std::string hostName = std::string())
    {
        api::ConnectRequest connectRequest;
        connectRequest.destinationHostName =
            hostName.empty() ? m_system.id : QByteArray(hostName.c_str());
        connectRequest.originatingPeerId = nx::utils::generateRandomName(7);
        connectRequest.connectSessionId = nx::utils::generateRandomName(7);
        connectRequest.connectionMethods = api::ConnectionMethod::proxy;
        m_mediatorClient->initiateConnection(
            connectRequest,
            [this](auto... args) { saveConnectResult(std::move(args)...); });
    }

    void whenConnectToUnknownPeerThroughHttp()
    {
        whenConnectToPeerOverHttp("unknown_host");
    }

    void thenSystemDisappearedFromListeningPeerList()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (;;)
        {
            auto [resultCode, listeningPeers] = getListeningPeers();
            ASSERT_EQ(api::ResultCode::ok, resultCode);
            if (listeningPeers.systems.find(m_system.id) == listeningPeers.systems.end())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thenOldConnectionIsClosedByMediator()
    {
        auto closedConnection = m_closeConnectionEvents.pop();
        ASSERT_EQ(closedConnection, m_serverConnections.front().get());
    }

    void thenConnectSucceeded()
    {
        thenConnectReported(api::ResultCode::ok);
    }

    void thenConnectReported(api::ResultCode expected)
    {
        ASSERT_EQ(expected, std::get<0>(m_connectResponseQueue.pop()));
    }

    nx::hpm::AbstractCloudDataProvider::System& system()
    {
        return m_system;
    }

    const nx::hpm::AbstractCloudDataProvider::System& system() const
    {
        return m_system;
    }

    nx::String serverId() const
    {
        return m_mediaServerEmulator->serverId();
    }

    nx::String fullServerName() const
    {
        return m_mediaServerEmulator->serverId() + "." + m_system.id;
    }

    void establishListeningConnectionToMediatorAsync()
    {
        using namespace std::placeholders;

        auto stunClient = std::make_shared<nx::network::stun::AsyncClient>();
        stunClient->connect(nx::network::url::Builder()
            .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(stunTcpEndpoint()));
        stunClient->setOnConnectionClosedHandler(
            std::bind(&ListeningPeer::saveConnectionClosedEvent, this, stunClient.get(), _1));
        auto connection = std::make_unique<nx::hpm::api::MediatorServerTcpConnection>(
            stunClient,
            this);
        m_serverConnections.push_back(std::move(stunClient));

        nx::hpm::api::ListenRequest request;
        request.systemId = m_system.id;
        request.serverId = m_serverId;
        auto connectionPtr = connection.get();
        connectionPtr->listen(
            request,
            [this, connection = std::move(connection)](
                nx::hpm::api::ResultCode resultCode,
                nx::hpm::api::ListenResponse response) mutable
            {
                m_listenResponseQueue.push(
                    std::make_tuple(resultCode, std::move(response)));
                connection.reset();
            });
    }

    void assertConnectionToMediatorHasBeenEstablished()
    {
        const auto result = m_listenResponseQueue.pop();
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, std::get<0>(result));
    }

private:
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_mediaServerEmulator;
    nx::String m_serverId;
    std::vector<std::shared_ptr<nx::network::stun::AsyncClient>> m_serverConnections;
    nx::utils::SyncQueue<nx::network::stun::AsyncClient*> m_closeConnectionEvents;
    nx::utils::SyncQueue<std::tuple<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>>
        m_listenResponseQueue;

    std::unique_ptr<api::Client> m_mediatorClient;
    nx::utils::SyncQueue<
        std::tuple<api::ResultCode, api::ConnectResponse>
    > m_connectResponseQueue;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_system = addRandomSystem();

        m_mediatorClient = std::make_unique<api::Client>(
            network::url::Builder().setScheme(network::http::kUrlSchemeName)
                .setEndpoint(httpEndpoint()).setPath(api::kMediatorApiPrefix));
    }

    void saveConnectionClosedEvent(
        nx::network::stun::AsyncClient* stunClient,
        SystemError::ErrorCode /*systemErrorCode*/)
    {
        m_closeConnectionEvents.push(stunClient);
    }

    void saveConnectResult(
        api::ResultCode resultCode, api::ConnectResponse response)
    {
        m_connectResponseQueue.push(
            std::make_tuple(resultCode, std::move(response)));
    }
};

TEST_F(ListeningPeer, connection_override)
{
    using namespace nx::hpm;

    givenListeningMediaServer();

    auto server2 = addServer(system(), serverId());
    ASSERT_NE(nullptr, server2);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server2->listen().first);

    // TODO #ak Checking that server2 connection has overridden server1
        // since both servers have same server id.

    auto dataLocker = moduleInstance()->impl()->listeningPeerPool()->
        findAndLockPeerDataByHostName(fullServerName());
    ASSERT_TRUE(static_cast<bool>(dataLocker));
    auto strongConnectionRef = dataLocker->value().peerConnection;
    ASSERT_NE(nullptr, strongConnectionRef);
    ASSERT_EQ(
        server2->mediatorConnectionLocalAddress(),
        strongConnectionRef->getSourceAddress());
    dataLocker.reset();
}

TEST_F(ListeningPeer, connection_is_closed_after_overriding)
{
    givenListeningServer();
    whenServerEstablishesNewConnection();
    thenOldConnectionIsClosedByMediator();
}

TEST_F(ListeningPeer, unknown_system_credentials)
{
    using namespace nx::hpm;

    auto system2 = addRandomSystem();
    system2.authKey.clear();    //< Making credentials invalid.
    auto server2 = addRandomServer(system2, boost::none, hpm::ServerTweak::noBindEndpoint);
    ASSERT_NE(nullptr, server2);
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->bind());
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->listen().first);
}

TEST_F(ListeningPeer, peer_disconnect)
{
    using namespace nx::hpm;

    givenListeningServer();

    auto [resultCode, listeningPeers] = getListeningPeers();
    ASSERT_EQ(api::ResultCode::ok, resultCode);
    ASSERT_EQ(1U, listeningPeers.systems.size());

    whenCloseConnectionToMediator();
    thenSystemDisappearedFromListeningPeerList();
}

TEST_F(ListeningPeer, can_connect_with_http_request)
{
    givenListeningServer();
    whenConnectToPeerOverHttp();
    thenConnectSucceeded();
}

TEST_F(ListeningPeer, connect_to_unknown_peer_through_http_request_results_not_found)
{
    whenConnectToUnknownPeerThroughHttp();
    thenConnectReported(api::ResultCode::notFound);
}

//-------------------------------------------------------------------------------------------------

class IteratableRelayClusterClient:
    public AbstractRelayClusterClient,
    public nx::network::aio::BasicPollable
{
public:
    virtual ~IteratableRelayClusterClient()
    {
        pleaseStopSync();
    }

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& /*peerId*/,
        RelayInstanceSearchCompletionHandler completionHandler) override
    {
        post(
            [this, completionHandler = std::move(completionHandler)]() mutable
            {
                m_selectRelayInstanceForListeningPeerHandler =
                    std::move(completionHandler);
                m_selectRelayInstanceForListeningPeerRequestQueue.push(0 /*dummy*/);
            });
    }

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& /*peerId*/,
        RelayInstanceSearchCompletionHandler /*completionHandler*/) override
    {
    }

    void waitNextSelectRelayInstanceForListeningPeerRequest()
    {
        m_selectRelayInstanceForListeningPeerRequestQueue.pop();
    }

    void resume()
    {
        post(
            [this]()
            {
                nx::utils::swapAndCall(
                    m_selectRelayInstanceForListeningPeerHandler,
                    cloud::relay::api::ResultCode::ok,
                    QUrl("http://some-relay-instance.com"));
            });
    }

private:
    RelayInstanceSearchCompletionHandler m_selectRelayInstanceForListeningPeerHandler;
    nx::utils::SyncQueue<int> m_selectRelayInstanceForListeningPeerRequestQueue;
};

class ListeningPeerStability:
    public ListeningPeer
{
public:
    ListeningPeerStability()
    {
        installIteratableRelayClusterClient();
    }

    virtual ~ListeningPeerStability()
    {
        if (m_factoryFuncBak)
        {
            RelayClusterClientFactory::instance()
                .setCustomFunc(std::move(*m_factoryFuncBak));
        }
    }

protected:
    void waitUntilRequestIsBeingProcessedByMediator()
    {
        m_relayClient->waitNextSelectRelayInstanceForListeningPeerRequest();
    }

    void unpauseRelayClusterClient()
    {
        m_relayClient->resume();
    }

private:
    std::optional<RelayClusterClientFactory::Function> m_factoryFuncBak;
    IteratableRelayClusterClient* m_relayClient = nullptr;

    void installIteratableRelayClusterClient()
    {
        m_factoryFuncBak = RelayClusterClientFactory::instance()
            .setCustomFunc(std::bind(&ListeningPeerStability::createRelayClusterClient, this));
    }

    std::unique_ptr<AbstractRelayClusterClient> createRelayClusterClient()
    {
        auto client = std::make_unique<IteratableRelayClusterClient>();
        m_relayClient = client.get();
        return client;
    }
};

TEST_F(ListeningPeerStability, listening_peer_breaks_connection_before_receiving_listen_response)
{
    establishListeningConnectionToMediatorAsync();

    waitUntilRequestIsBeingProcessedByMediator();

    whenCloseConnectionToMediator();
    unpauseRelayClusterClient();

    /* Process does not crash. */
}

} // namespace test
} // namespace hpm
} // namespace nx
