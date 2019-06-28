#include "basic_component_test.h"

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

BasicComponentTest::BasicComponentTest(Mode /*mode*/):
    base_type("traffic_relay", QString())
{
    NX_ASSERT(m_discoveryServer.bindAndListen());
    m_relayCluster = std::make_unique<TrafficRelayCluster>(m_discoveryServer.url());
}

void BasicComponentTest::addRelayInstance(
    std::vector<const char*> args,
    bool waitUntilStarted)
{
    auto& relay = m_relayCluster->addRelay();
    for (const auto& arg : args)
        relay.addArg(arg);

    if (waitUntilStarted)
    {
        ASSERT_TRUE(relay.startAndWaitUntilStarted());
    }
    else
    {
        relay.start();
    }
}

Relay& BasicComponentTest::relay(int index)
{
    return m_relayCluster->relay(index);
}

const Relay& BasicComponentTest::relay(int index) const
{
    return m_relayCluster->relay(index);
}

void BasicComponentTest::stopAllInstances()
{
    m_relayCluster->stopAllRelays();
}

bool BasicComponentTest::peerInformationSynchronizedInCluster(
    const std::string& hostname)
{
    return m_relayCluster->peerInformationSynchronizedInCluster(hostname);
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
