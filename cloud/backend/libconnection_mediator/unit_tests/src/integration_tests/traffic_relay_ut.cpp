#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/future.h>

#include "mediator_relay_integration_test_setup.h"

namespace nx {
namespace hpm {
namespace test {

class TrafficRelay:
    public MediatorRelayIntegrationTestSetup
{
protected:
    void givenMultipleTrafficRelayUrls()
    {
        // Base type constructor adds a url, so only one more is needed.
        addTrafficRelayUrl("http://traffic-relay-test.com");
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(TrafficRelay, listen_reports_taffic_relay_url)
{
    issueListenRequest();
    assertTrafficRelayUrlHasBeenReported();
}

TEST_F(TrafficRelay, listen_reports_multiple_relay_urls)
{
    givenMultipleTrafficRelayUrls();
    issueListenRequest();
    assertTrafficRelayUrlHasBeenReported();
}

TEST_F(TrafficRelay, connect_reports_traffic_relay_url)
{
    givenListeningServer();
    issueConnectRequest();
    assertTrafficRelayUrlHasBeenReported();
}

} // namespace test
} // namespace hpm
} // namespace nx
