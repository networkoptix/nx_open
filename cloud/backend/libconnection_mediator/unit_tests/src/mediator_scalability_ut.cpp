#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

#include "mediator_cluster.h"

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";
static constexpr int kMaxMediators = 2;

} // namespace

class MediatorScalability:
    public testing::Test,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    ~MediatorScalability()
    {
        for (auto& connection : m_serverConnections)
            connection->pleaseStopSync();

        m_mediatorClient.reset();
    }

    virtual std::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = m_system.id;
        systemCredentials.serverId = m_mediaServer->serverId();
        systemCredentials.key = m_system.authKey;
        return systemCredentials;
    }

protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_discoveryServer.bindAndListen());
    }

    void addMediator()
    {
        std::string nodeId = lm("mediator_%1").arg(m_mediatorCluster.size()).toStdString();
        std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();
        std::string maxMediatorCount = std::to_string(kMaxMediators);

        auto& mediator = m_mediatorCluster.addMediator({
            "-server/name", "127.0.0.1",
            "-listeningPeerDb/enabled", "true",
            "-discovery/enabled", "true",
            "-discovery/discoveryServiceUrl", discoveryServiceUrl.c_str(),
            "-discovery/roundTripPadding", "1ms",
            "-discovery/registrationErrorDelay", "10ms",
            "-discovery/onlineNodesRequestDelay", "10ms",
            "-p2pDb/clusterId", kClusterId,
            "-p2pDb/nodeId", nodeId.c_str(),
            "-p2pDb/maxConcurrentConnectionsFromSystem", maxMediatorCount.c_str()});

        ASSERT_TRUE(mediator.startAndWaitUntilStarted());
    }

    void givenMultipleMediators()
    {
        for (int i = 0; i < kMaxMediators; ++i)
            addMediator();
    }

    void givenSynchronizedClusterWithServer()
    {
        givenMultipleMediators();

        whenAddServer();

        thenServerInfoIsSynchronized();
    }

    void whenAddServer(int mediatorIndex = 0)
    {
        m_system = m_mediatorCluster.mediator(mediatorIndex).addRandomSystem();
        m_mediaServer = m_mediatorCluster.mediator(mediatorIndex).addRandomServer(m_system);
        m_mediaServerFullName = m_mediaServer->fullName().toStdString();
    }

    void whenMediaServerGoesOffline()
    {
        m_mediaServer.reset();
    }

    void thenServerInfoIsSynchronized()
    {
        while (!m_mediatorCluster.peerInformationSynchronizedInCluster(m_mediaServerFullName))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void thenServerInfoIsDroppedFromCluster()
    {
        while (!m_mediatorCluster.peerInformationIsAbsentFromCluster(m_mediaServerFullName))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void givenServerIsListeningOnMediator()
    {
        establishListeningConnectionToMediatorAsync();
        assertConnectionToMediatorHasBeenEstablished();
    }

    void whenTryToConnectToServerOnDifferentMediator()
    {
        // m_mediaServer is connected to m_mediatorCluster.mediator(0);
        auto& differentMediator = m_mediatorCluster.mediator(1);

        m_mediatorClient = std::make_unique<api::Client>(
            network::url::Builder().setScheme(network::http::kUrlSchemeName)
            .setEndpoint(differentMediator.httpEndpoint()).setPath(api::kMediatorApiPrefix));

        api::ConnectRequest connectRequest;
        connectRequest.destinationHostName = m_mediaServer->fullName();
        connectRequest.originatingPeerId = nx::utils::generateRandomName(7);
        connectRequest.connectSessionId = nx::utils::generateRandomName(7);
        connectRequest.connectionMethods = api::ConnectionMethod::proxy;
        m_mediatorClient->initiateConnection(
            connectRequest,
            [this](api::ResultCode resultCode, api::ConnectResponse response)
            {
                m_connectResponseQueue.push(
                    std::make_tuple(resultCode, std::move(response)));
            });
    }

    void thenRequestIsRedirected()
    {
        auto resultCodeAndResponse = m_connectResponseQueue.pop();
        while (std::get<api::ResultCode>(resultCodeAndResponse) != api::ResultCode::ok)
            resultCodeAndResponse = m_connectResponseQueue.pop();
    }

    void andConnectionToDifferentMediatorIsEstablished()
    {
        //TODO
    }

private:
    void establishListeningConnectionToMediatorAsync()
    {
        auto& mediator = m_mediatorCluster.mediator(0);

        auto stunClient = std::make_shared<nx::network::stun::AsyncClient>();

        stunClient->connect(nx::network::url::Builder()
            .setScheme(nx::network::stun::kUrlSchemeName)
            .setEndpoint(mediator.stunTcpEndpoint()));

        stunClient->setOnConnectionClosedHandler(
            [this, stunClient = stunClient.get()](SystemError::ErrorCode /*systemErrorCode*/)
            {
                m_closeConnectionEvents.push(stunClient);
            });

        auto connection =
            std::make_unique<nx::hpm::api::MediatorServerTcpConnection>(stunClient, this);
        m_serverConnections.push_back(std::move(stunClient));

        nx::hpm::api::ListenRequest request;
        request.systemId = m_system.id;
        request.serverId = m_mediaServer->serverId();

        auto connectionPtr = connection.get();
        connectionPtr->listen(
            std::move(request),
            [this, connection = std::move(connection)](
                nx::hpm::api::ResultCode resultCode,
                nx::hpm::api::ListenResponse response) mutable
            {
                m_listenResponseQueue.push(std::make_tuple(resultCode, std::move(response)));
                connection.reset();
            });
    }

    void assertConnectionToMediatorHasBeenEstablished()
    {
        const auto result = m_listenResponseQueue.pop();
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, std::get<0>(result));
    }

private:
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    MediatorCluster m_mediatorCluster;
    std::unique_ptr<MediaServerEmulator> m_mediaServer;
    AbstractCloudDataProvider::System m_system;
    std::string m_mediaServerFullName;

    std::vector<std::shared_ptr<nx::network::stun::AsyncClient>> m_serverConnections;
    nx::utils::SyncQueue<nx::network::stun::AsyncClient*> m_closeConnectionEvents;
    nx::utils::SyncQueue<std::tuple<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>>
        m_listenResponseQueue;

    std::unique_ptr<api::Client> m_mediatorClient;
    nx::utils::SyncQueue<
        std::tuple<api::ResultCode, api::ConnectResponse>
    > m_connectResponseQueue;
};


TEST_F(MediatorScalability, server_info_synchronizes_in_cluster)
{
    givenMultipleMediators();

    whenAddServer();

    thenServerInfoIsSynchronized();
}

TEST_F(MediatorScalability, server_info_is_dropped_after_connection_closure)
{
    givenSynchronizedClusterWithServer();

    whenMediaServerGoesOffline();

    thenServerInfoIsDroppedFromCluster();
}

TEST_F(MediatorScalability, connect_request_redirected_to_correct_mediator)
{
    givenSynchronizedClusterWithServer();
    givenServerIsListeningOnMediator();

    whenTryToConnectToServerOnDifferentMediator();

    thenRequestIsRedirected();

    andConnectionToDifferentMediatorIsEstablished();
}

} // namespace nx::hpm::test