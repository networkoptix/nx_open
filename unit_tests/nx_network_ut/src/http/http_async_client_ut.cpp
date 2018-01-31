#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/synchronous_tcp_server.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx_http {
namespace test {

static const QString kTestPath("/HttpAsyncClient_upgrade");
static const nx_http::StringType kUpgradeTo("NXRELAY/0.1");
static const nx::Buffer kNewProtocolMessage("Hello, Http Client!");

class UpgradableHttpServer:
    public nx::network::test::SynchronousTcpServer
{
protected:
    virtual void processConnection(AbstractStreamSocket* connection) override
    {
        readRequest(connection);
        sendResponse(connection);
    }

private:
    void readRequest(AbstractStreamSocket* connection)
    {
        nx::Buffer buf;
        buf.resize(4096);

        connection->recv(buf.data(), buf.size());
    }

    void sendResponse(AbstractStreamSocket* connection)
    {
        // Sending response.
        const char* responseMessage =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: NXRELAY/0.1\r\n"
            "Connection: Upgrade\r\n"
            "\r\n";

        nx::Buffer response = responseMessage + kNewProtocolMessage;

        ASSERT_EQ(
            response.size(),
            connection->send(response.constData(), response.size()));
    }
};

class HttpAsyncClient:
    public ::testing::Test
{
protected:
    void initializeServerThatDoesNotSendUpgradeHeaderInResponse()
    {
        m_serverSendUpgradeHeaderInResponse = false;
    }

    void givenSynchronousServer()
    {
        m_synchronousServer = std::make_unique<UpgradableHttpServer>();
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void whenPerformedUpgrade()
    {
        m_httpClient.doUpgrade(
            prepareUrl(),
            kUpgradeTo,
            std::bind(&HttpAsyncClient::onUpgradeDone, this));
    }

    void whenPerformedSuccessfulUpgrade()
    {
        whenPerformedUpgrade();
        auto responseContext = m_upgradeResponses.pop();
        assertUpgradeResponseIsCorrect(responseContext);
        m_upgradedConnection = std::move(responseContext.connection);
    }

    void thenUpgradeRequestIsCorrect()
    {
        const auto httpRequest = m_upgradeRequests.pop();
        ASSERT_EQ(nx_http::Method::options, httpRequest.requestLine.method);
        ASSERT_EQ("Upgrade", nx_http::getHeaderValue(httpRequest.headers, "Connection"));
        ASSERT_EQ(kUpgradeTo, nx_http::getHeaderValue(httpRequest.headers, "Upgrade"));
    }

    void thenConnectionHasBeenUpgraded()
    {
        auto responseContext = m_upgradeResponses.pop();
        assertUpgradeResponseIsCorrect(responseContext);
    }

    void thenRequestHasFailed()
    {
        auto responseContext = m_upgradeResponses.pop();
        ASSERT_FALSE(responseContext.statusCode);
    }

    void thenTrafficSentByServerAfterResponseIsAvailableOnSocket()
    {
        nx::Buffer buf;
        buf.reserve(kNewProtocolMessage.size());
        const int bytesRead =
            m_upgradedConnection->recv(buf.data(), buf.capacity(), MSG_WAITALL);
        buf.resize(bytesRead);

        ASSERT_EQ(kNewProtocolMessage.size(), bytesRead);
        ASSERT_EQ(kNewProtocolMessage, buf);
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
    std::unique_ptr<AbstractStreamSocket> m_upgradedConnection;
    std::unique_ptr<UpgradableHttpServer> m_synchronousServer;

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
        else
            response->headers.emplace("Upgrade", nx_http::StringType());
        response->headers.emplace("Connection", "Upgrade");

        completionHandler(nx_http::StatusCode::switchingProtocols);
    }

    QUrl prepareUrl()
    {
        const auto serverAddress = m_synchronousServer
            ? m_synchronousServer->endpoint()
            : m_httpServer.serverAddress();
        return nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(serverAddress).setPath(kTestPath);
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

    void assertUpgradeResponseIsCorrect(
        const ResponseContext& responseContext)
    {
        ASSERT_EQ(nx_http::StatusCode::switchingProtocols, responseContext.statusCode);
        ASSERT_NE(nullptr, responseContext.connection);
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

TEST_F(HttpAsyncClient, data_after_successfull_upgrade_is_not_lost)
{
    givenSynchronousServer();
    whenPerformedSuccessfulUpgrade();
    thenTrafficSentByServerAfterResponseIsAvailableOnSocket();
}

TEST(HttpAsyncClientTypes, lexicalSerialization)
{
    ASSERT_EQ(QnLexical::serialized(nx_http::AsyncHttpClient::AuthType::authBasicAndDigest).toStdString(),
        std::string("authBasicAndDigest"));
    ASSERT_EQ(QnLexical::serialized(nx_http::AsyncHttpClient::AuthType::authBasic).toStdString(),
        std::string("authBasic"));
    ASSERT_EQ(QnLexical::serialized(nx_http::AsyncHttpClient::AuthType::authDigest).toStdString(),
        std::string("authDigest"));

    ASSERT_EQ(QnLexical::deserialized<nx_http::AsyncHttpClient::AuthType>("authBasicAndDigest"),
        nx_http::AsyncHttpClient::AuthType::authBasicAndDigest);
    ASSERT_EQ(QnLexical::deserialized<nx_http::AsyncHttpClient::AuthType>("authDigest"),
        nx_http::AsyncHttpClient::AuthType::authDigest);
    ASSERT_EQ(QnLexical::deserialized<nx_http::AsyncHttpClient::AuthType>("authBasic"),
        nx_http::AsyncHttpClient::AuthType::authBasic);
}

} // namespace test
} // namespace nx_http
