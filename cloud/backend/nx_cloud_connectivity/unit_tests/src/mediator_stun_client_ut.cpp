#include <gtest/gtest.h>

#include <nx/network/cloud/mediator_stun_client.h>
#include <nx/network/stun/async_client.h>
#include <nx/network/test_support/stun_simple_server.h>
#include <nx/network/test_support/stun_async_client_acceptance_tests.h>

namespace nx::hpm::api::test {

class MediatorStunClient:
    public nx::hpm::api::MediatorStunClient
{
    using base_type = nx::hpm::api::MediatorStunClient;

public:
    MediatorStunClient(AbstractAsyncClient::Settings settings):
        base_type(
            std::move(settings),
            /*MediatorEndpointProvider*/ nullptr)
    {
    }
};

//-------------------------------------------------------------------------------------------------

struct MediatorStunClientTestTypes
{
    using ClientType = MediatorStunClient;
    using ServerType = nx::network::stun::test::SimpleServer;
};

using namespace nx::network::stun::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    MediatorStunClient,
    StunAsyncClientAcceptanceTest,
    MediatorStunClientTestTypes);

//-------------------------------------------------------------------------------------------------

class MediatorStunClientKeepAlive:
    public StunAsyncClientAcceptanceTest<MediatorStunClientTestTypes>
{
public:
    MediatorStunClientKeepAlive()
    {
        enableProxy();

        nx::network::KeepAliveOptions keepAliveOptions;
        keepAliveOptions.inactivityPeriodBeforeFirstProbe = std::chrono::milliseconds(10);
        keepAliveOptions.probeSendPeriod = std::chrono::milliseconds(10);
        keepAliveOptions.probeCount = 3;
        client().setKeepAliveOptions(keepAliveOptions);
    }
};

TEST_F(MediatorStunClientKeepAlive, connection_is_closed_after_keep_alive_failure)
{
    givenConnectedClient();
    whenSilentlyDropAllConnectionsToServer();
    thenClientReportedConnectionClosureWithResult(SystemError::connectionReset);
}

} // namespace nx::hpm::api::test
