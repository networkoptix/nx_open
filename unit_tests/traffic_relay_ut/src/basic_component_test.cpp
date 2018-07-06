#include "basic_component_test.h"

#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

BasicComponentTest::BasicComponentTest(QString tmpDir):
    utils::test::TestWithTemporaryDirectory("traffic_relay", tmpDir)
{
    addArg("--http/listenOn=127.0.0.1:0");

    controller::PublicIpDiscoveryService::setDiscoverFunc(
        []() {return network::HostAddress("127.0.0.1"); });
}

BasicComponentTest::~BasicComponentTest()
{
    stop();
}

nx::utils::Url BasicComponentTest::basicUrl() const
{
    return nx::network::url::Builder().setScheme("http").setHost("127.0.0.1")
        .setPort(moduleInstance()->httpEndpoints()[0].port).toUrl();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
