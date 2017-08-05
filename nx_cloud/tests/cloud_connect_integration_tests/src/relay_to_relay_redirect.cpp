#include <vector>
#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stun/async_client.h>
#include <nx/utils/thread/sync_queue.h>

#include <libconnection_mediator/src/test_support/mediator_functional_test.h>
#include <libtraffic_relay/src/test_support/traffic_relay_launcher.h>


namespace nx {
namespace network {
namespace cloud {
namespace test {


using Relay = nx::cloud::relay::test::Launcher;
using RelayList = std::vector<Relay>;

class RelayToRelayRedirect: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        startRelays();
    }


private:
    RelayList m_relays;
    nx::hpm::MediatorFunctionalTest m_mediator;

    void startRelays()
    {
    }
};

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx