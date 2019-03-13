#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include "api_test_fixture.h"

namespace nx {
namespace hpm {
namespace test {

class StunApi:
    public ApiTestFixture<nx::network::stun::AsyncClient>
{
protected:
    virtual nx::utils::Url stunApiUrl() const override
    {
        return nx::network::url::Builder().setScheme(nx::network::stun::kUrlSchemeName)
            .setEndpoint(stunTcpEndpoint());
    }
};

TEST_F(StunApi, inactive_connection_is_not_closed_without_inactivity_timeout)
{
    openInactiveConnection();
    assertConnectionIsNotClosedDuring(std::chrono::seconds(1));
}

//-------------------------------------------------------------------------------------------------

class StunApiLowInactivityTimeout:
    public StunApi
{
public:
    StunApiLowInactivityTimeout()
    {
        addArg("-stun/connectionInactivityTimeout", "100ms");
    }
};

TEST_F(StunApiLowInactivityTimeout, inactive_connection_is_closed_by_timeout)
{
    openInactiveConnection();
    waitForConnectionIsClosedByMediator();
}

TEST_F(StunApiLowInactivityTimeout, malformed_request_is_properly_handled)
{
    whenSendMalformedRequest();

    thenReponseIsReceived();
    andResponseCodeIs(api::ResultCode::badRequest);

    waitForConnectionIsClosedByMediator();
}

} // namespace test
} // namespace hpm
} // namespace nx
