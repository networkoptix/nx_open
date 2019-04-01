#include <gtest/gtest.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>
#include <nx/cloud/mediator/public_ip_discovery.h>

#include "mediator_cluster.h"
#include "override_public_ip.h"

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";
static constexpr int kMaxMediators = 4;

} // namespace

class MediatorScalability:
    public testing::Test,
    OverridePublicIp
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_discoveryServer.bindAndListen());
    }

    void addMediator(bool addServerName = false)
    {
        std::string nodeId = lm("mediator_%1").arg(m_mediatorCluster.size()).toStdString();
        std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();
        std::string maxMediatorCount = std::to_string(kMaxMediators);

        auto& mediator = m_mediatorCluster.addMediator({
            "-discovery/enabled", "true",
            "-discovery/discoveryServiceUrl", discoveryServiceUrl.c_str(),
            "-discovery/roundTripPadding", "1ms",
            "-discovery/registrationErrorDelay", "10ms",
            "-discovery/onlineNodesRequestDelay", "10ms",
            "-p2pDb/clusterId", kClusterId,
            "-p2pDb/nodeId", nodeId.c_str(),
            "-p2pDb/maxConcurrentConnectionsFromSystem", maxMediatorCount.c_str(),
            "-clusterDbMap/enabled", "true"});

        if (addServerName)
            mediator.addArg("-server/name", nodeId.c_str());

        ASSERT_TRUE(mediator.startAndWaitUntilStarted());
    }

    void givenMultipleMediators()
    {
        for (int i = 0; i < kMaxMediators; ++i)
            addMediator();
    }

    void givenSynchronizedCluster()
    {
        givenMultipleMediators();

        whenAddSingleServer();

        thenServerInfoIsSynchronized();
    }

    void whenAddSingleServer()
    {
        int r = rand() % m_mediatorCluster.size();
        auto system = m_mediatorCluster.mediator(r).addRandomSystem();
        m_mediaServer = m_mediatorCluster.mediator(r).addRandomServer(system);
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

private:
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    MediatorCluster m_mediatorCluster;
    std::unique_ptr<MediaServerEmulator> m_mediaServer;
    std::string m_mediaServerFullName;
};


TEST_F(MediatorScalability, server_info_synchronizes_in_cluster)
{
    givenMultipleMediators();

    whenAddSingleServer();

    thenServerInfoIsSynchronized();
}

TEST_F(MediatorScalability, server_info_is_dropped_after_connection_closure)
{
    givenSynchronizedCluster();

    whenMediaServerGoesOffline();

    thenServerInfoIsDroppedFromCluster();
}

} // namespace nx::hpm::test