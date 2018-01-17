#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/stun/async_client_with_http_tunneling.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include <http/http_api_path.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class StunOverHttpTunnelling:
    public MediatorFunctionalTest
{
public:
    StunOverHttpTunnelling():
        m_stunClient(nx::network::stun::AbstractAsyncClient::Settings())
    {
    }

    ~StunOverHttpTunnelling()
    {
        m_stunClient.pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_stunClient.connect(
            nx::network::url::Builder().setScheme("http")
                .setEndpoint(httpEndpoint()).setPath(http::kStunOverHttpTunnelPath).toUrl(),
            [](SystemError::ErrorCode) {});
    }

    void whenIssueStunRequestThroughHttpTunnel()
    {
        using namespace std::placeholders;
        using namespace nx::network::stun;

        Message request(Header(
            MessageClass::request,
            MethodType::bindingMethod));

        m_stunClient.sendRequest(
            request,
            std::bind(&StunOverHttpTunnelling::onStunResponse, this, _1, _2));
    }

    void thenRequestIsProcessed()
    {
        auto response = m_messagesReceived.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(response));
        ASSERT_EQ(
            nx::network::stun::MessageClass::successResponse,
            std::get<1>(response).header.messageClass);
    }

private:
    nx::network::stun::AsyncClientWithHttpTunneling m_stunClient;
    nx::utils::SyncQueue<std::tuple<SystemError::ErrorCode, nx::network::stun::Message>> m_messagesReceived;

    void onStunResponse(
        SystemError::ErrorCode sysErrorCode,
        nx::network::stun::Message responseMessage)
    {
         m_messagesReceived.push(std::make_tuple(sysErrorCode, std::move(responseMessage)));
    }
};

TEST_F(StunOverHttpTunnelling, requests_are_processed)
{
    whenIssueStunRequestThroughHttpTunnel();
    thenRequestIsProcessed();
}

} // namespace test
} // namespace hpm
} // namespace nx
