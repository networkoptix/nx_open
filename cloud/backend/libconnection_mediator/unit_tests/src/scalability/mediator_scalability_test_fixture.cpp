#include "mediator_scalability_test_fixture.h"

namespace nx::hpm::test {

MediatorScalabilityTestFixture::~MediatorScalabilityTestFixture()
{
    for (auto& connection : m_serverConnections)
        connection->pleaseStopSync();
}

std::optional<nx::hpm::api::SystemCredentials> MediatorScalabilityTestFixture::getSystemCredentials() const
{
    nx::hpm::api::SystemCredentials systemCredentials;
    systemCredentials.systemId = m_system.id;
    systemCredentials.serverId = m_mediaServer->serverId();
    systemCredentials.key = m_system.authKey;
    return systemCredentials;
}

void MediatorScalabilityTestFixture::SetUp()
{
    ASSERT_TRUE(m_discoveryServer.bindAndListen());
}

void MediatorScalabilityTestFixture::addMediator()
{
    std::string nodeId = lm("mediator_%1").arg(m_mediatorCluster.size()).toStdString();
    std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();
    std::string maxMediatorCount = std::to_string(kMaxMediators);

    auto& mediator = m_mediatorCluster.addMediator({
        "-https/listenOn", "127.0.0.1",
        "-http/connectionInactivityTimeout", "10m",
        "-server/name", "127.0.0.1",
        "-listeningPeerDb/connectionRetryDelay", "100ms",
        "-listeningPeerDb/cluster/discovery/enabled", "true",
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl", discoveryServiceUrl.c_str(),
        "-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms",
        "-listeningPeerDb/cluster/discovery/registrationErrorDelay", "10ms",
        "-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "10ms",
        "-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms",
        "-listeningPeerDb/cluster/clusterId", kClusterId,
        "-listeningPeerDb/cluster/nodeId", nodeId.c_str(),
        "-listeningPeerDb/cluster/maxConcurrentConnectionsFromSystem", maxMediatorCount.c_str()});

    ASSERT_TRUE(mediator.startAndWaitUntilStarted());
}

void MediatorScalabilityTestFixture::givenMultipleMediators()
{
    for (int i = 0; i < kMaxMediators; ++i)
        addMediator();
}

void MediatorScalabilityTestFixture::givenSynchronizedClusterWithListeningServer()
{
    givenMultipleMediators();

    whenAddServer();

    thenServerInfoIsSynchronized();
}

void MediatorScalabilityTestFixture::whenAddServer()
{
    m_system = m_mediatorCluster.mediator(0).addRandomSystem();
    m_mediaServer = m_mediatorCluster.mediator(0).addRandomServer(m_system);
    m_mediaServerFullName = m_mediaServer->fullName().toStdString();

    auto resultCodeAndResponse = m_mediaServer->listen();
    ASSERT_EQ(api::ResultCode::ok, resultCodeAndResponse.first);
}

void MediatorScalabilityTestFixture::whenMediaServerGoesOffline()
{
    m_mediaServer.reset();
}

void MediatorScalabilityTestFixture::thenServerInfoIsSynchronized()
{
    while (!m_mediatorCluster.peerInformationSynchronizedInCluster(m_mediaServerFullName))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void MediatorScalabilityTestFixture::thenServerInfoIsDroppedFromCluster()
{
    while (!m_mediatorCluster.peerInformationIsAbsentFromCluster(m_mediaServerFullName))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void MediatorScalabilityTestFixture::thenRequestIsRedirected()
{
    auto resultCodeAndResponse = m_connectResponseQueue.pop();
    ASSERT_EQ(api::ResultCode::ok, std::get<api::ResultCode>(resultCodeAndResponse));
}

} // namespace nx::hpm::test