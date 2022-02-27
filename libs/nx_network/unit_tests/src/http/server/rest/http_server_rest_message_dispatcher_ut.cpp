// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../http_server_basic_message_dispatcher_ut.h"

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::network::http::server::test {

INSTANTIATE_TYPED_TEST_SUITE_P(
    HttpServerRestMessageDispatcher,
    HttpServerBasicMessageDispatcher,
    nx::network::http::server::rest::MessageDispatcher);

class HttpServerRestMessageDispatcher:
    public HttpServerBasicMessageDispatcher<rest::MessageDispatcher>
{
protected:
    void givenRegisteredHandler(const std::string& pathTemplate)
    {
        registerHandler(pathTemplate);
    }

    void whenSentRequest(const std::string& requestPath)
    {
        assertRequestIsDispatched(requestPath, nx::network::http::Method::get);
    }

    void thenParametersHaveBeenPassedToTheHandler(
        const RequestPathParams& expectedParams)
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
    thenParametersHaveBeenPassedToTheHandler(
        {{{"accountId", "akolesnikov"}, {"systemName", "la_office_test"}}});
}

TEST_F(HttpServerRestMessageDispatcher, find_handler_by_url_with_query)
{
    givenRegisteredHandler("/account/{accountId}/systems");
    whenSentRequest("/account/akolesnikov/systems?status=online");
    thenParametersHaveBeenPassedToTheHandler(
        {{{"accountId", "akolesnikov"}}});
}

} // namespace nx::network::http::server::test
