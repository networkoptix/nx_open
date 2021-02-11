#include <variant>

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
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

static constexpr char kTestPath[] = "/HttpAsyncClient_upgrade";
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

static constexpr char kExtraResponseHandlerPath[] = "/HttpClientTest/extraResponseMessage/";

class HttpAsyncClient:
    public ::testing::Test
{
public:
    HttpAsyncClient():
        m_proxyHost(lm("%1.com").args(QnUuid::createUuid().toSimpleString()).toStdString())
    {
        m_credentials = Credentials("username", PasswordAuthToken("password"));

        createClient();
    }

    ~HttpAsyncClient()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    TestHttpServer m_httpServer;
    std::unique_ptr<AsyncClient> m_client;
    nx::utils::SyncQueue<nx::network::http::Request> m_receivedRequests;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            "",
            std::bind(&HttpAsyncClient::processHttpRequest, this, _1, _2));

        m_httpServer.registerRequestProcessorFunc(
            kExtraResponseHandlerPath,
            [this](auto&&... args)
            {
                handleExtraResponseMessageRequest(std::forward<decltype(args)>(args)...);
            });

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void sendRequest(const nx::utils::Url& url)
    {
        m_client->doGet(url, [this]() { saveResponse(); });
    }

    nx::utils::Url prepareUrl(const std::string& requestPath = std::string())
    {
        const auto serverAddress = m_synchronousServer
            ? m_synchronousServer->endpoint()
            : m_httpServer.serverAddress();
        return nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(serverAddress).setPath(requestPath);
    }

    void createClient()
    {
        if (m_client)
        {
            m_client->pleaseStopSync();
            m_client.reset();
        }

        m_client = std::make_unique<AsyncClient>();
        m_client->setResponseReadTimeout(kNoTimeout);
        m_client->setMessageBodyReadTimeout(kNoTimeout);
        m_client->setCredentials(m_credentials);
    }

    void enableAuthentication()
    {
        m_httpServer.setAuthenticationEnabled(true);
        m_httpServer.registerUserCredentials(m_credentials);
    }

    void disablePersistentServerConnections()
    {
        m_httpServer.server().setPersistentConnectionEnabled(false);
    }

    void givenSynchronousServer()
    {
        m_synchronousServer = std::make_unique<UpgradableHttpServer>();
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void whenSendConnectRequest()
    {
        m_client->doConnect(
            prepareUrl("/connect_test"),
            m_proxyHost.c_str(),
            [this]() { saveResponse(); });
    }

    void whenSendToBrokenHttpServerThatSendsExtraResponseMessages()
    {
        m_client->doGet(
            prepareUrl(kExtraResponseHandlerPath),
            [this]() { saveResponse(); });
    }

    void thenCorrectConnectRequestIsReceived()
    {
        const auto requestReceived = m_receivedRequests.pop();

        ASSERT_EQ(nx::network::http::Method::connect, requestReceived.requestLine.method);
        ASSERT_EQ(m_proxyHost, requestReceived.requestLine.url.path().toStdString());
        ASSERT_EQ(m_proxyHost, requestReceived.requestLine.url.toString().toStdString());
    }

    void thenRequestSucceeded()
    {
        const auto requestResult = m_responses.pop();
        ASSERT_EQ(SystemError::noError, requestResult.systemResultCode);
        ASSERT_EQ(StatusCode::ok, requestResult.response.statusLine.statusCode);
    }

private:
    struct RequestResult
    {
        SystemError::ErrorCode systemResultCode = SystemError::noError;
        nx::network::http::Response response;
    };

    std::unique_ptr<UpgradableHttpServer> m_synchronousServer;
    std::string m_proxyHost;
    nx::utils::SyncQueue<RequestResult> m_responses;
    Credentials m_credentials;

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_receivedRequests.push(std::move(requestContext.request));
        completionHandler(nx::network::http::StatusCode::ok);
    }

    void handleExtraResponseMessageRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        const auto authHeaderIter =
            requestContext.request.headers.find(header::Authorization::NAME);
        if (authHeaderIter != requestContext.request.headers.end())
        {
            if (header::DigestAuthorization authorizationHeader;
                authorizationHeader.parse(authHeaderIter->second))
            {
                if (validateAuthorization(
                        requestContext.request.requestLine.method,
                        m_credentials,
                        authorizationHeader))
                {
                    completionHandler(http::StatusCode::ok);
                    return;
                }
            }
        }

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params.emplace("nonce", "not_random_nonce");
        wwwAuthenticate.params.emplace("realm", "realm_of_possibility");
        wwwAuthenticate.params.emplace("algorithm", "MD5");

        requestContext.response->headers.emplace(
            header::WWWAuthenticate::NAME,
            wwwAuthenticate.serialized());

        requestContext.response->headers.emplace("Content-Type", "text/plain");
        requestContext.response->headers.emplace("Content-Length", "0");
        requestContext.response->headers.emplace("Connection", "close");
        requestContext.response->messageBody =
            "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";

        completionHandler(StatusCode::unauthorized);
    }

    void saveResponse()
    {
        m_responses.push(RequestResult{
            m_client->lastSysErrorCode(),
            m_client->response() ? *m_client->response() : http::Response()});
    }
};

TEST_F(HttpAsyncClient, connect_method)
{
    whenSendConnectRequest();
    thenCorrectConnectRequestIsReceived();
}

TEST_F(HttpAsyncClient, connect_with_authentication)
{
    enableAuthentication();

    whenSendConnectRequest();
    thenRequestSucceeded();
}

TEST_F(HttpAsyncClient, clears_receive_buffer_when_opening_new_connection)
{
    whenSendToBrokenHttpServerThatSendsExtraResponseMessages();
    thenRequestSucceeded();
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
        m_client->doUpgrade(
            prepareUrl(kTestPath),
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
        response.connection = m_client->takeSocket();
        if (!m_client->failed())
        {
            response.statusCode = static_cast<nx::network::http::StatusCode::Value>(
                m_client->response()->statusLine.statusCode);
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

//-------------------------------------------------------------------------------------------------

class HttpAsyncClientAuthorization:
    public HttpAsyncClient
{
    using base_type = HttpAsyncClient;

    static constexpr char kPath[] = "/HttpAsyncClientAuthorization/1";

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_credentials = Credentials("n", PasswordAuthToken("x"));
    }

    void givenServerWithDigestAuthentication()
    {
        m_httpServer.registerRequestProcessorFunc(
            kPath,
            [this](auto&&... args) { processRequest(std::forward<decltype(args)>(args)...); });
    }

    void whenIssueRequest()
    {
        createClient();
        m_client->setCredentials(m_credentials);

        sendRequest(prepareUrl(kPath));
    }

    void thenRequestSucceeded()
    {
        base_type::thenRequestSucceeded();

        m_lastClientAuthorization = m_authorizationReceived.pop();
    }

    void andAuthenticationWasPassedInOneStep()
    {
        ASSERT_EQ(0, m_lastClientAuthorization->requestNumberInConnection);
    }

    void andCachedServerNonceWasUsed()
    {
        ASSERT_TRUE(m_lastClientAuthorization->repeatedNonceIsUsed);
    }

    void andNonceCountInTheLastRequestWas(int expected)
    {
        auto ncIter = m_lastClientAuthorization->header.digest->params.find("nc");
        ASSERT_NE(m_lastClientAuthorization->header.digest->params.end(), ncIter);
        ASSERT_EQ(expected, ncIter->second.toInt(nullptr, 16));
    }

private:
    struct ClientAuthorization
    {
        header::Authorization header;
        int requestNumberInConnection = 0;
        bool repeatedNonceIsUsed = false;
    };

    Credentials m_credentials;
    nx::utils::SyncQueue<ClientAuthorization> m_authorizationReceived;
    std::map<StringType, int /*use count*/> m_generatedNonces;
    std::optional<ClientAuthorization> m_lastClientAuthorization;

private:
    void processRequest(
        RequestContext ctx,
        RequestProcessedHandler completionHandler)
    {
        const auto authzStr = getHeaderValue(ctx.request.headers, "Authorization");
        if (!authzStr.isEmpty())
        {
            header::DigestAuthorization authorization;
            if (authorization.parse(authzStr) &&
                authorization.authScheme == header::AuthScheme::digest &&
                validateAuthorization(
                    ctx.request.requestLine.method,
                    m_credentials,
                    authorization))
            {
                const auto nonceUseCount =
                    ++m_generatedNonces[authorization.digest->params["nonce"]];

                m_authorizationReceived.push(ClientAuthorization{
                    authorization,
                    ctx.connection->messagesReceivedCount() - 1,
                    nonceUseCount > 1});
                return completionHandler(StatusCode::ok);
            }
        }

        http::RequestResult result(StatusCode::unauthorized);

        const auto nonce = QnUuid::createUuid().toSimpleByteArray();
        m_generatedNonces.emplace(nonce, 0);

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params["nonce"] = nonce;
        wwwAuthenticate.params["realm"] = "nx";
        wwwAuthenticate.params["algorithm"] = "MD5";
        wwwAuthenticate.params["qop"] = "auth";
        ctx.response->headers.emplace(wwwAuthenticate.NAME, wwwAuthenticate.serialized());

        completionHandler(std::move(result));
    }
};

TEST_F(HttpAsyncClientAuthorization, server_nonce_is_cached_and_reused)
{
    givenServerWithDigestAuthentication();

    whenIssueRequest();
    thenRequestSucceeded();

    whenIssueRequest();
    thenRequestSucceeded();
    andAuthenticationWasPassedInOneStep();
    andCachedServerNonceWasUsed();
    andNonceCountInTheLastRequestWas(2);
}

//-------------------------------------------------------------------------------------------------

namespace {

class TransparentProxyHandler:
    public server::proxy::AbstractProxyHandler
{
public:
    TransparentProxyHandler(const SocketAddress& target):
        m_target(target)
    {
    }

protected:
    virtual void detectProxyTarget(
        const HttpServerConnection& /*connection*/,
        Request* const request,
        ProxyTargetDetectedHandler handler)
    {
        // Replacing the Host header with the proxy target as specified in [rfc7230; 5.4].
        request->headers.erase("Host");
        request->headers.emplace("Host", m_target.toString().toUtf8());

        handler(StatusCode::ok, m_target);
    }

private:
    const SocketAddress m_target;
};

} // namespace

class HttpAsyncClientProxy:
    public ::testing::Test
{
public:
    HttpAsyncClientProxy():
        m_proxy(server::Role::proxy),
        m_transparentProxy(server::Role::proxy)
    {
    }

    ~HttpAsyncClientProxy()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    static constexpr char kResourcePath[] = "/HttpAsyncClientProxy/resource";
    static constexpr char kResource[] = "HttpAsyncClientProxy test resource";

    virtual void SetUp() override
    {
        m_resourceServer.registerStaticProcessor(
            kResourcePath,
            kResource,
            "text/plain");

        m_resourceServer.setAuthenticationEnabled(true);

        m_userCredentials.username = "n";
        m_userCredentials.authToken = PasswordAuthToken("x");
        m_resourceServer.registerUserCredentials(m_userCredentials);

        ASSERT_TRUE(m_resourceServer.bindAndListen());

        // Launching proxy server.
        m_proxy.registerRequestProcessor<server::proxy::ProxyHandler>(kAnyPath);
        ASSERT_TRUE(m_proxy.bindAndListen());

        m_transparentProxy.registerRequestProcessor<TransparentProxyHandler>(
            kAnyPath,
            [this]()
            {
                return std::make_unique<TransparentProxyHandler>(
                    m_resourceServer.serverAddress());
            });
        ASSERT_TRUE(m_transparentProxy.bindAndListen());
    }

    void enableAuthorizationOnProxy()
    {
        m_proxyCredentials = Credentials();
        m_proxyCredentials->username = "p";
        m_proxyCredentials->authToken = PasswordAuthToken("h");

        m_proxy.setAuthenticationEnabled(true);
        m_proxy.registerUserCredentials(*m_proxyCredentials);

        m_transparentProxy.setAuthenticationEnabled(true);
        m_transparentProxy.registerUserCredentials(*m_proxyCredentials);
    }

    void whenRequestingResourceThroughProxy()
    {
        m_client = std::make_unique<AsyncClient>();

        if (m_proxyCredentials)
            m_client->setProxyUserCredentials(*m_proxyCredentials);
        m_client->setProxyVia(m_proxy.serverAddress(), false);

        m_client->setUserCredentials(m_userCredentials);
        m_client->doGet(
            getUrl(kResourcePath),
            [this]() { saveResponse(); });
    }

    void whenRequestingResourceThroughTransparentProxy()
    {
        m_client = std::make_unique<AsyncClient>();
        m_client->setUserCredentials(m_userCredentials);
        if (m_proxyCredentials)
            m_client->setProxyUserCredentials(*m_proxyCredentials);

        m_client->doGet(
            url::Builder(getUrl(kResourcePath))
                .setEndpoint(m_transparentProxy.serverAddress()),
            [this]() { saveResponse(); });
    }

    nx::utils::Url getUrl(const std::string_view&)
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_resourceServer.serverAddress())
            .setPath(kResourcePath).toUrl();
    }

    void thenResourceIsDelivered()
    {
        auto result = m_responseQueue.pop();
        if (result.index() == 0)
            FAIL() << SystemError::toString(std::get<0>(result)).toStdString();

        auto response = std::get<Response>(result);
        ASSERT_EQ(StatusCode::ok, response.statusLine.statusCode);
        ASSERT_EQ(kResource, response.messageBody);
    }

private:
    nx::network::http::TestHttpServer m_resourceServer;
    nx::network::http::TestHttpServer m_proxy;
    nx::network::http::TestHttpServer m_transparentProxy;
    std::unique_ptr<AsyncClient> m_client;
    Credentials m_userCredentials;
    std::optional<Credentials> m_proxyCredentials;
    nx::utils::SyncQueue<std::variant<SystemError::ErrorCode, Response>> m_responseQueue;

private:
    void saveResponse()
    {
        if (m_client->failed())
            return m_responseQueue.push(m_client->lastSysErrorCode());

        auto response = *m_client->response();
        response.messageBody += m_client->fetchMessageBodyBuffer();
        m_responseQueue.push(std::move(response));
    }
};

TEST_F(HttpAsyncClientProxy, proxy_authorization_works)
{
    enableAuthorizationOnProxy();

    whenRequestingResourceThroughProxy();
    thenResourceIsDelivered();
}

// "Transparent proxying" is a proxying when client is not aware it is actually
// interacting with a proxy.
TEST_F(HttpAsyncClientProxy, authorization_on_a_transparent_proxy)
{
    enableAuthorizationOnProxy();

    whenRequestingResourceThroughTransparentProxy();
    thenResourceIsDelivered();
}

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
