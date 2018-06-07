#include <gtest/gtest.h>

#include "basic_component_test.h"

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>

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
    return nx::network::url::Builder().setScheme("http").setHost("127.0.0.1")
        .setPort(moduleInstance()->httpEndpoints()[0].port).toUrl();
}

//-------------------------------------------------------------------------------------------------

BasicComponentTest::BasicComponentTest(QString tmpDir):
    utils::test::TestWithTemporaryDirectory("traffic_relay", tmpDir)
{
    controller::PublicIpDiscoveryService::setDiscoverFunc(
        []() {return network::HostAddress("127.0.0.1"); });
}

void BasicComponentTest::addRelayInstance(
    std::vector<const char*> args,
    bool waitUntilStarted)
{
    m_relays.push_back(std::make_unique<Relay>());
    for (const auto& arg : args)
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

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
