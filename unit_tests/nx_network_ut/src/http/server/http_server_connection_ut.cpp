#include <memory>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx_http {
namespace test {

static const char* const kTestPath = "/HttpServerConnectionTest";

class HttpServerConnection:
    public ::testing::Test
{
protected:
    void whenRespondWithEmptyMessageBody()
    {
        installRequestHandlerWithEmptyBody();
        performRequest(kUrlSchemeName);
    }

    void whenIssuedRequestViaSsl()
    {
        performRequest(kSecureUrlSchemeName);
    }

    void whenIssuedRequestViaRawTcp()
    {
        performRequest(kUrlSchemeName);
    }

    void whenReceiveStrictTransportSecurityHeaderInResponse()
    {
        whenIssuedRequestViaSsl();
        thenResponseContainsHeader(header::StrictTransportSecurity::NAME);
    }

    void thenOnResponseSentHandlerIsCalled()
    {
        m_responseSentEvents.pop();
    }

    void thenResponseContainsHeader(const nx::String& headerName)
    {
        ASSERT_TRUE(m_prevResponse->headers.find(headerName) != m_prevResponse->headers.end());
    }

    void thenResponseDoesNotContainHeader(const nx::String& headerName)
    {
        ASSERT_TRUE(m_prevResponse->headers.find(headerName) == m_prevResponse->headers.end());
    }

    void thenStrictTransportSecurityHeaderIsValid()
    {
        const auto strictTransportSecurityStr = getHeaderValue(
            m_prevResponse->headers,
            header::StrictTransportSecurity::NAME);
        ASSERT_TRUE(m_prevStrictTransportSecurity.parse(strictTransportSecurityStr));
    }
    
    void andStrictTransportSecurityMaxAgeIsNotLessThan(
        std::chrono::seconds desiredMinimalAge)
    {
        ASSERT_GE(m_prevStrictTransportSecurity.maxAge, desiredMinimalAge);
    }

private:
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<int /*dummy*/> m_responseSentEvents;
    boost::optional<Response> m_prevResponse;
    header::StrictTransportSecurity m_prevStrictTransportSecurity;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void installRequestHandlerWithEmptyBody()
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpServerConnection::provideEmptyMessageBody, this, _1, _2, _3, _4, _5));
    }

    void performRequest(const char* urlScheme)
    {
        HttpClient client;
        const auto url = nx::network::url::Builder()
            .setScheme(urlScheme).setEndpoint(m_httpServer.serverAddress()).setPath(kTestPath);
        ASSERT_TRUE(client.doGet(url));
        m_prevResponse = *client.response();
    }

    void provideEmptyMessageBody(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        Request /*request*/,
        Response* const /*response*/,
        RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        RequestResult result(StatusCode::ok);
        result.connectionEvents.onResponseHasBeenSent =
            std::bind(&HttpServerConnection::onResponseSent, this, _1);
        result.dataSource = std::make_unique<EmptyMessageBodySource>(
            "text/plain",
            boost::none);
        completionHandler(std::move(result));
    }

    void onResponseSent(nx_http::HttpServerConnection* /*connection*/)
    {
        m_responseSentEvents.push(0);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(
    HttpServerConnection,
    on_response_sent_handler_is_called_after_response_with_empty_body)
{
    whenRespondWithEmptyMessageBody();
    thenOnResponseSentHandlerIsCalled();
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_present_in_case_of_ssl_is_used)
{
    whenIssuedRequestViaSsl();
    thenResponseContainsHeader(header::StrictTransportSecurity::NAME);
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_absent_in_case_of_ssl_is_used)
{
    whenIssuedRequestViaRawTcp();
    thenResponseDoesNotContainHeader(header::StrictTransportSecurity::NAME);
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_valid)
{
    whenReceiveStrictTransportSecurityHeaderInResponse();

    thenStrictTransportSecurityHeaderIsValid();
    andStrictTransportSecurityMaxAgeIsNotLessThan(std::chrono::hours(24) * 365);
}

} // namespace test
} // namespace nx_http
