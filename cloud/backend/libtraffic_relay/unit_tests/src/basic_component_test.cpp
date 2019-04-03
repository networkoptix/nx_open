#include "basic_component_test.h"

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>


namespace nx {
namespace cloud {
namespace relay {
namespace test {

namespace {

static constexpr char kClusterId[] = "relay_service_test_cluster";

} //namespace

Relay::Relay()
{
    addArg("--http/listenOn=127.0.0.1:0");
}

Relay::~Relay()
{
    stop();
}

nx::utils::Url Relay::basicUrl() const
{
    return nx::network::url::Builder()
        .setScheme("http").setHost("127.0.0.1")
        .setPort(moduleInstance()->httpEndpoints()[0].port).toUrl();
}

//-------------------------------------------------------------------------------------------------

BasicComponentTest::BasicComponentTest(Mode /*mode*/):
    base_type("traffic_relay", QString())
{
    using namespace std::placeholders;

    controller::PublicIpDiscoveryService::setDiscoverFunc(
        []() { return network::HostAddress("127.0.0.1"); });

    m_serverListening = m_discoveryServer.bindAndListen();
}

void BasicComponentTest::addRelayInstance(
    std::vector<const char*> args,
    bool waitUntilStarted)
{
    m_relays.push_back(std::make_unique<Relay>());
    for (const auto& arg: args)
        m_relays.back()->addArg(arg);

    ASSERT_TRUE(m_serverListening);
    m_relays.back()->addArg("-listeningPeerDb/connectionRetryDelay", "1ms");
    m_relays.back()->addArg("-discovery/registrationErrorDelay", "1ms");
    m_relays.back()->addArg("-discovery/onlineNodesRequestDelay", "100ms");
    m_relays.back()->addArg("-discovery/enabled", "true");
    m_relays.back()->addArg("-discovery/roundTripPadding", "1ms");
    m_relays.back()->addArg(
        "-discovery/discoveryServiceUrl",
        m_discoveryServer.url().toStdString().c_str());
    m_relays.back()->addArg("-p2pDb/clusterId", kClusterId);
    m_relays.back()->addArg(
        "-p2pDb/maxConcurrentConnectionsFromSystem",
        std::to_string(7).c_str());

    if (waitUntilStarted)
    {
        ASSERT_TRUE(m_relays.back()->startAndWaitUntilStarted());
    }
    else
    {
        m_relays.back()->start();
    }
}

Relay& BasicComponentTest::relay(int index)
{
    return *m_relays[index];
}

const Relay& BasicComponentTest::relay(int index) const
{
    return *m_relays[index];
}

void BasicComponentTest::stopAllInstances()
{
    for (auto& relay: m_relays)
        relay->stop();
}

bool BasicComponentTest::peerInformationSynchronizedInCluster(
    const std::string& hostname)
{
    const auto doesRelayhaveHostname =
        [](const Relay* relay, const std::string& hostname)->bool
        {
            nx::utils::SyncQueue<bool> hasHostname;
            relay->moduleInstance()->remoteRelayPeerPool().findRelayByDomain(
                hostname,
                [&hasHostname](std::string relay){ hasHostname.push(!relay.empty()); });
            return hasHostname.pop();
    };

    bool allRelaysHaveHostname = true;
    for (const auto& relay : m_relays)
        allRelaysHaveHostname &= doesRelayhaveHostname(relay.get(), hostname);

    return allRelaysHaveHostname;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
