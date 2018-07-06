#include "../http_server_basic_message_dispatcher_ut.h"

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpServerRestMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx::network::http::server::rest::MessageDispatcher);

class HttpServerRestMessageDispatcher:
    public HttpServerBasicMessageDispatcher<rest::MessageDispatcher>
{
protected:
    void givenRegisteredHandler(const nx::network::http::StringType& pathTemplate)
    {
        registerHandler(pathTemplate);
    }

    void whenSentRequest(const nx::network::http::StringType& requestPath)
    {
        assertRequestIsDispatched(requestPath, nx::network::http::Method::get);
    }

    void thenParametersHaveBeenPassedToTheHandler(
        const std::vector<nx::network::http::StringType>& expectedParams)
    {
        ASSERT_EQ(expectedParams, issuedRequest().requestPathParams);
    }
};

TEST_F(
    HttpServerRestMessageDispatcher,
    parameters_matched_in_request_path_are_given_to_the_handler)
{
    givenRegisteredHandler("/account/{accountId}/system/{systemName}");
    whenSentRequest("/account/akolesnikov/system/la_office_test");
    thenParametersHaveBeenPassedToTheHandler({"akolesnikov", "la_office_test"});
}

TEST_F(HttpServerRestMessageDispatcher, find_handler_by_url_with_query)
{
    givenRegisteredHandler("/account/{accountId}/systems");
    whenSentRequest("/account/akolesnikov/systems?status=online");
    thenParametersHaveBeenPassedToTheHandler({ "akolesnikov" });
}

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
