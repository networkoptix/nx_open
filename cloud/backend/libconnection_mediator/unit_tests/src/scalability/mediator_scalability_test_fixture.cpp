#include "mediator_scalability_test_fixture.h"

namespace nx::hpm::test {

MediatorScalability::~MediatorScalability()
{
    for (auto& connection : m_serverConnections)
        connection->pleaseStopSync();
}

std::optional<nx::hpm::api::SystemCredentials> MediatorScalability::getSystemCredentials() const
{
    nx::hpm::api::SystemCredentials systemCredentials;
    systemCredentials.systemId = m_system.id;
    systemCredentials.serverId = m_mediaServer->serverId();
    systemCredentials.key = m_system.authKey;
    return systemCredentials;
}

void MediatorScalability::SetUp()
{
    ASSERT_TRUE(m_discoveryServer.bindAndListen());
}

void MediatorScalability::addMediator()
{
    std::string nodeId = lm("mediator_%1").arg(m_mediatorCluster.size()).toStdString();
    std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();
    std::string maxMediatorCount = std::to_string(kMaxMediators);

    auto& mediator = m_mediatorCluster.addMediator({
        "-https/listenOn", "127.0.0.1",
        "-http/connectionInactivityTimeout", "10m",
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

void MediatorScalability::givenMultipleMediators()
{
    for (int i = 0; i < kMaxMediators; ++i)
        addMediator();
}

void MediatorScalability::givenSynchronizedClusterWithServer()
{
    givenMultipleMediators();

    whenAddServer();

    thenServerInfoIsSynchronized();
}

void MediatorScalability::givenServerIsListeningOnMediator(int mediatorIndex)
{
    establishListeningConnectionToMediatorAsync(mediatorIndex);
    assertConnectionToMediatorHasBeenEstablished();
}

void MediatorScalability::whenAddServer(int mediatorIndex)
{
    m_system = m_mediatorCluster.mediator(mediatorIndex).addRandomSystem();
    m_mediaServer = m_mediatorCluster.mediator(mediatorIndex).addRandomServer(m_system);
    m_mediaServerFullName = m_mediaServer->fullName().toStdString();
}

void MediatorScalability::whenMediaServerGoesOffline()
{
    m_mediaServer.reset();
}

void MediatorScalability::thenServerInfoIsSynchronized()
{
    while (!m_mediatorCluster.peerInformationSynchronizedInCluster(m_mediaServerFullName))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void MediatorScalability::thenServerInfoIsDroppedFromCluster()
{
    while (!m_mediatorCluster.peerInformationIsAbsentFromCluster(m_mediaServerFullName))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void MediatorScalability::establishListeningConnectionToMediatorAsync(int mediatorIndex)
{
    auto& mediator = m_mediatorCluster.mediator(mediatorIndex);

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

void MediatorScalability::assertConnectionToMediatorHasBeenEstablished()
{
    const auto result = m_listenResponseQueue.pop();
    ASSERT_EQ(nx::hpm::api::ResultCode::ok, std::get<0>(result));
}

} // namespace nx::hpm::test