#include "basic_component_test.h"

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

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

BasicComponentTest::BasicComponentTest(Mode mode):
    utils::test::TestWithTemporaryDirectory("traffic_relay", QString())
{
    using namespace std::placeholders;

    controller::PublicIpDiscoveryService::setDiscoverFunc(
        []() {return network::HostAddress("127.0.0.1"); });

    if (mode == Mode::cluster)
    {
        m_factoryFunctionBak =
            model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
                std::bind(&BasicComponentTest::createRemoteRelayPeerPool, this, _1));
    }
}

BasicComponentTest::~BasicComponentTest()
{
    if (m_factoryFunctionBak)
    {
        model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
            std::move(*m_factoryFunctionBak));
    }
}

void BasicComponentTest::addRelayInstance(
    std::vector<const char*> args,
    bool waitUntilStarted)
{
    m_relays.push_back(std::make_unique<Relay>());
    for (const auto& arg: args)
        m_relays.back()->addArg(arg);

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
    return !m_listeningPeerPool.findRelay(hostname).empty();
}

std::unique_ptr<model::AbstractRemoteRelayPeerPool>
    BasicComponentTest::createRemoteRelayPeerPool(
        const conf::Settings& /*settings*/)
{
    return std::make_unique<RemoteRelayPeerPool>(&m_listeningPeerPool);
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
