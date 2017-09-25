#include <memory>

#include <gtest/gtest.h>

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
public:

protected:
    void whenRespondWithEmptyMessageBody()
    {
        installRequestHandlerWithEmptyBody();
        performRequest();
    }

    void thenOnResponseSentHandlerIsCalled()
    {
        m_responseSentEvents.pop();
    }

private:
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<int /*dummy*/> m_responseSentEvents;

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

    void performRequest()
    {
        nx_http::HttpClient client;
        const auto url = nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName).setEndpoint(m_httpServer.serverAddress())
            .setPath(kTestPath);
        ASSERT_TRUE(client.doGet(url));
    }

    void provideEmptyMessageBody(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        nx_http::RequestResult result(nx_http::StatusCode::ok);
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

TEST_F(
    HttpServerConnection,
    on_response_sent_handler_is_called_after_response_with_empty_body)
{
    whenRespondWithEmptyMessageBody();
    thenOnResponseSentHandlerIsCalled();
}

} // namespace test
} // namespace nx_http
