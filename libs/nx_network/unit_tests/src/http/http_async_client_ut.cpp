#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/synchronous_tcp_server.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace network {
namespace http {
namespace test {

static const QString kTestPath("/HttpAsyncClient_upgrade");
static const nx::network::http::StringType kUpgradeTo("NXRELAY/0.1");
static const nx::Buffer kNewProtocolMessage("Hello, Http Client!");

class UpgradableHttpServer:
    public nx::network::test::SynchronousStreamSocketServer
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

//-------------------------------------------------------------------------------------------------

class HttpAsyncClient:
    public ::testing::Test
{
public:
    HttpAsyncClient():
        m_proxyHost(lm("%1.com").args(QnUuid::createUuid().toSimpleString()).toStdString())
    {
    }

    ~HttpAsyncClient()
    {
        m_httpClient.pleaseStopSync();
    }

protected:
    TestHttpServer m_httpServer;
    AsyncClient m_httpClient;
    nx::utils::SyncQueue<nx::network::http::Request> m_receivedRequests;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            "",
            std::bind(&HttpAsyncClient::processHttpRequest, this, _1, _2));

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    nx::utils::Url prepareUrl()
    {
        const auto serverAddress = m_synchronousServer
            ? m_synchronousServer->endpoint()
            : m_httpServer.serverAddress();
        return nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(serverAddress).setPath(kTestPath);
    }

    void givenSynchronousServer()
    {
        m_synchronousServer = std::make_unique<UpgradableHttpServer>();
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void whenSendConnectRequest()
    {
        m_httpClient.doConnect(
            prepareUrl(),
            m_proxyHost.c_str(),
            std::bind(&HttpAsyncClient::saveResponse, this));
    }

    void thenCorrectConnectRequestIsReceived()
    {
        const auto requestReceived = m_receivedRequests.pop();

        ASSERT_EQ(nx::network::http::Method::connect, requestReceived.requestLine.method);
        ASSERT_EQ(m_proxyHost, requestReceived.requestLine.url.path().toStdString());
        ASSERT_EQ(m_proxyHost, requestReceived.requestLine.url.toString().toStdString());
    }

private:
    std::unique_ptr<UpgradableHttpServer> m_synchronousServer;
    std::string m_proxyHost;

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_receivedRequests.push(std::move(requestContext.request));
        completionHandler(nx::network::http::StatusCode::ok);
    }

    void saveResponse()
    {
        // TODO
    }
};

TEST_F(HttpAsyncClient, connect_method)
{
    whenSendConnectRequest();
    thenCorrectConnectRequestIsReceived();
}

//-------------------------------------------------------------------------------------------------

class HttpAsyncClientConnectionUpgrade:
    public HttpAsyncClient
{
    using base_type = HttpAsyncClient;

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
            std::bind(&HttpAsyncClientConnectionUpgrade::onUpgradeDone, this));
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
        const auto httpRequest = m_receivedRequests.pop();
        ASSERT_EQ(nx::network::http::Method::options, httpRequest.requestLine.method);
        ASSERT_EQ("Upgrade", nx::network::http::getHeaderValue(httpRequest.headers, "Connection"));
        ASSERT_EQ(kUpgradeTo, nx::network::http::getHeaderValue(httpRequest.headers, "Upgrade"));
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
        boost::optional<nx::network::http::StatusCode::Value> statusCode;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    nx::utils::SyncQueue<ResponseContext> m_upgradeResponses;
    bool m_serverSendUpgradeHeaderInResponse = true;
    std::unique_ptr<AbstractStreamSocket> m_upgradedConnection;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpAsyncClientConnectionUpgrade::processHttpRequest, this, _1, _2));

        base_type::SetUp();
    }

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_receivedRequests.push(std::move(requestContext.request));

        if (m_serverSendUpgradeHeaderInResponse)
            requestContext.response->headers.emplace("Upgrade", kUpgradeTo);
        else
            requestContext.response->headers.emplace("Upgrade", nx::network::http::StringType());
        requestContext.response->headers.emplace("Connection", "Upgrade");

        completionHandler(nx::network::http::StatusCode::switchingProtocols);
    }

    void onUpgradeDone()
    {
        ResponseContext response;
        response.connection = m_httpClient.takeSocket();
        if (!m_httpClient.failed())
        {
            response.statusCode = static_cast<nx::network::http::StatusCode::Value>(
                m_httpClient.response()->statusLine.statusCode);
        }
        m_upgradeResponses.push(std::move(response));
    }

    void assertUpgradeResponseIsCorrect(
        const ResponseContext& responseContext)
    {
        ASSERT_EQ(nx::network::http::StatusCode::switchingProtocols, responseContext.statusCode);
        ASSERT_NE(nullptr, responseContext.connection);
    }
};

TEST_F(HttpAsyncClientConnectionUpgrade, upgrade_successful)
{
    whenPerformedUpgrade();
    thenUpgradeRequestIsCorrect();
    thenConnectionHasBeenUpgraded();
}

TEST_F(HttpAsyncClientConnectionUpgrade, upgrade_missing_protocol_in_response)
{
    initializeServerThatDoesNotSendUpgradeHeaderInResponse();

    whenPerformedUpgrade();
    thenRequestHasFailed();
}

TEST_F(HttpAsyncClientConnectionUpgrade, data_after_successfull_upgrade_is_not_lost)
{
    givenSynchronousServer();
    whenPerformedSuccessfulUpgrade();
    thenTrafficSentByServerAfterResponseIsAvailableOnSocket();
}

//-------------------------------------------------------------------------------------------------

TEST(HttpAsyncClientTypes, lexicalSerialization)
{
    ASSERT_EQ(QnLexical::serialized(nx::network::http::AuthType::authBasicAndDigest).toStdString(),
        std::string("authBasicAndDigest"));
    ASSERT_EQ(QnLexical::serialized(nx::network::http::AuthType::authBasic).toStdString(),
        std::string("authBasic"));
    ASSERT_EQ(QnLexical::serialized(nx::network::http::AuthType::authDigest).toStdString(),
        std::string("authDigest"));

    ASSERT_EQ(QnLexical::deserialized<nx::network::http::AuthType>("authBasicAndDigest"),
        nx::network::http::AuthType::authBasicAndDigest);
    ASSERT_EQ(QnLexical::deserialized<nx::network::http::AuthType>("authDigest"),
        nx::network::http::AuthType::authDigest);
    ASSERT_EQ(QnLexical::deserialized<nx::network::http::AuthType>("authBasic"),
        nx::network::http::AuthType::authBasic);
}

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
