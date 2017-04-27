#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx_http {
namespace test {

static const QString kTestPath("/HttpAsyncClient_upgrade");
static const nx_http::StringType kUpgradeTo("NXRELAY/0.1");

class HttpAsyncClient:
    public ::testing::Test
{
protected:
    void initializeServerThatDoesNotSendUpgradeHeaderInResponse()
    {
        m_serverSendUpgradeHeaderInResponse = false;
    }

    void whenPerformedUpgrade()
    {
        m_httpClient.doUpgrade(
            prepareUrl(),
            kUpgradeTo,
            std::bind(&HttpAsyncClient::onUpgradeDone, this));
    }

    void thenUpgradeRequestIsCorrect()
    {
        const auto httpRequest = m_upgradeRequests.pop();
        ASSERT_EQ(nx_http::Method::OPTIONS, httpRequest.requestLine.method);
        ASSERT_EQ("Upgrade", nx_http::getHeaderValue(httpRequest.headers, "Connection"));
        ASSERT_EQ(kUpgradeTo, nx_http::getHeaderValue(httpRequest.headers, "Upgrade"));
    }

    void thenConnectionHasBeenUpgraded()
    {
        auto responseContext = m_upgradeResponses.pop();
        ASSERT_EQ(nx_http::StatusCode::switchingProtocols, *responseContext.statusCode);
        ASSERT_NE(nullptr, responseContext.connection);
    }

    void thenRequestHasFailed()
    {
        auto responseContext = m_upgradeResponses.pop();
        ASSERT_FALSE(responseContext.statusCode);
    }

private:
    struct ResponseContext
    {
        boost::optional<nx_http::StatusCode::Value> statusCode;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    TestHttpServer m_httpServer;
    AsyncClient m_httpClient;
    nx::utils::SyncQueue<nx_http::Request> m_upgradeRequests;
    nx::utils::SyncQueue<ResponseContext> m_upgradeResponses;
    bool m_serverSendUpgradeHeaderInResponse = true;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpAsyncClient::processHttpRequest, this, _1, _2, _3, _4, _5));
        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    virtual void TearDown() override
    {
        m_httpClient.pleaseStopSync();
    }

    void processHttpRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler)
    {
        m_upgradeRequests.push(std::move(request));

        if (m_serverSendUpgradeHeaderInResponse)
            response->headers.emplace("Upgrade", kUpgradeTo);
        response->headers.emplace("Connection", "Upgrade");
        completionHandler(nx_http::StatusCode::switchingProtocols);
    }

    QUrl prepareUrl()
    {
        return nx::network::url::Builder().setScheme("http")
            .setEndpoint(m_httpServer.serverAddress()).setPath(kTestPath);
    }

    void onUpgradeDone()
    {
        ResponseContext response;
        response.connection = m_httpClient.takeSocket();
        if (!m_httpClient.failed())
        {
            response.statusCode = static_cast<nx_http::StatusCode::Value>(
                m_httpClient.response()->statusLine.statusCode);
        }
        m_upgradeResponses.push(std::move(response));
    }
};

TEST_F(HttpAsyncClient, upgrade_successful)
{
    whenPerformedUpgrade();
    thenUpgradeRequestIsCorrect();
    thenConnectionHasBeenUpgraded();
}

TEST_F(HttpAsyncClient, upgrade_missing_protocol_in_response)
{
    initializeServerThatDoesNotSendUpgradeHeaderInResponse();

    whenPerformedUpgrade();
    thenRequestHasFailed();
}

} // namespace test
} // namespace nx_http
