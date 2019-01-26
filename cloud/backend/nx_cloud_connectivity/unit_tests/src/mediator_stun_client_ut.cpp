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
            /*MediatorEndpointProvider*/ nullptr,
            std::make_unique<nx::network::stun::AsyncClient>(std::move(settings)))
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

// TODO: #ak Add keep-alive related tests.

} // namespace nx::hpm::api::test
