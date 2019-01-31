#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/http/tunneling/server.h>
#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/http/tunneling/detail/connection_upgrade_tunnel_client.h>
#include <nx/network/http/tunneling/detail/get_post_tunnel_client.h>
#include <nx/network/http/tunneling/detail/experimental_tunnel_client.h>
#include <nx/network/http/tunneling/detail/ssl_tunnel_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::tunneling::test {

enum TunnelMethod
{
    getPost = 0x01,
    connectionUpgrade = 0x02,
    experimental = 0x04,
    ssl = 0x08,

    all = getPost | connectionUpgrade | experimental | ssl,
};

static constexpr char kBasePath[] = "/HttpTunnelingTest";

class HttpTunneling:
    public ::testing::TestWithParam<int /*Tunnel methods mask*/>
{
public:
    HttpTunneling():
        m_tunnelingServer(
            std::bind(&HttpTunneling::saveServerTunnel, this, std::placeholders::_1),
            nullptr)
    {
        m_clientFactoryBak = detail::ClientFactory::instance().setCustomFunc(
            [this](const nx::utils::Url& baseUrl)
            {
                return m_localFactory.create(baseUrl);
            });

        enableTunnelMethods(GetParam());
    }

    ~HttpTunneling()
    {
        detail::ClientFactory::instance().setCustomFunc(
            std::move(m_clientFactoryBak));

        if (m_tunnelingClient)
            m_tunnelingClient->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        startHttpServer();

        m_tunnelingServer.registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());
    }

    void stopTunnelingServer()
    {
        m_httpServer->server().pleaseStopSync();
    }

    void setEstablishTunnelTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }

    void givenSilentTunnellingServer()
    {
        stopTunnelingServer();
        startHttpServer();

        m_httpServer->registerRequestProcessorFunc(
            http::kAnyPath,
            [this](auto&&... args) { leaveRequestWithoutResponse(std::move(args)...); });
    }

    void whenRequestTunnel()
    {
        m_tunnelingClient = std::make_unique<Client>(m_baseUrl);
        if (m_timeout)
            m_tunnelingClient->setTimeout(*m_timeout);
        else
            m_tunnelingClient->setTimeout(nx::network::kNoTimeout);

        m_tunnelingClient->openTunnel(
            [this](auto&&... args) { saveClientTunnel(std::move(args)...); });
    }

    void thenTunnelIsEstablished()
    {
        thenClientTunnelSucceeded();

        thenServerTunnelSucceded();

        assertDataExchangeWorksThroughTheTunnel();
    }

    void thenClientTunnelSucceeded()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_EQ(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_TRUE(StatusCode::isSuccessCode(m_prevClientTunnelResult.httpStatus));
        ASSERT_NE(nullptr, m_prevClientTunnelResult.connection);
    }

    void thenServerTunnelSucceded()
    {
        m_prevServerTunnelConnection = m_serverTunnels.pop();
        ASSERT_NE(nullptr, m_prevServerTunnelConnection);
    }

    void thenTunnelIsNotEstablished()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_NE(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_EQ(nullptr, m_prevClientTunnelResult.connection);
    }

    void andResultCodeIs(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_prevClientTunnelResult.sysError);
    }

private:
    Server<> m_tunnelingServer;
    std::unique_ptr<Client> m_tunnelingClient;
    detail::ClientFactory m_localFactory;
    detail::ClientFactory::Function m_clientFactoryBak;
    std::unique_ptr<TestHttpServer> m_httpServer;
    nx::utils::SyncQueue<OpenTunnelResult> m_clientTunnels;
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverTunnels;
    OpenTunnelResult m_prevClientTunnelResult;
    nx::utils::Url m_baseUrl;
    std::unique_ptr<AbstractStreamSocket> m_prevServerTunnelConnection;
    std::optional<std::chrono::milliseconds> m_timeout;

    void startHttpServer()
    {
        m_httpServer = std::make_unique<TestHttpServer>();

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::network::url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath);
    }

    void enableTunnelMethods(int tunnelMethodMask)
    {
        if ((tunnelMethodMask & TunnelMethod::all) == TunnelMethod::all)
            return; //< By default, the factory is initialized with all methods.

        m_localFactory.clear();

        if (tunnelMethodMask & TunnelMethod::getPost)
            m_localFactory.registerClientType<detail::GetPostTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::connectionUpgrade)
            m_localFactory.registerClientType<detail::ConnectionUpgradeTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::experimental)
            m_localFactory.registerClientType<detail::ExperimentalTunnelClient>();

        if (tunnelMethodMask & TunnelMethod::ssl)
            m_localFactory.registerClientType<detail::SslTunnelClient>();
    }

    void saveClientTunnel(OpenTunnelResult result)
    {
        m_clientTunnels.push(std::move(result));
    }

    void saveServerTunnel(std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_serverTunnels.push(std::move(connection));
    }

    void assertDataExchangeWorksThroughTheTunnel()
    {
        ASSERT_TRUE(m_prevServerTunnelConnection->setNonBlockingMode(false));
        ASSERT_TRUE(m_prevClientTunnelResult.connection->setNonBlockingMode(false));

        assertDataCanBeTransferred(
            m_prevServerTunnelConnection.get(),
            m_prevClientTunnelResult.connection.get());

        assertDataCanBeTransferred(
            m_prevClientTunnelResult.connection.get(),
            m_prevServerTunnelConnection.get());
    }

    void assertDataCanBeTransferred(
        AbstractStreamSocket* from,
        AbstractStreamSocket* to)
    {
        constexpr char buf[] = "Hello, world!";
        ASSERT_EQ(sizeof(buf), from->send(buf, sizeof(buf)));

        char readBuf[sizeof(buf)];
        ASSERT_EQ(sizeof(readBuf), to->recv(readBuf, sizeof(readBuf)));

        ASSERT_EQ(0, memcmp(buf, readBuf, sizeof(readBuf)));
    }

    void leaveRequestWithoutResponse(
        http::RequestContext /*requestContext*/,
        http::RequestProcessedHandler /*completionHandler*/)
    {
    }
};

//-------------------------------------------------------------------------------------------------

TEST_P(HttpTunneling, tunnel_is_established)
{
    whenRequestTunnel();
    thenTunnelIsEstablished();
}

TEST_P(HttpTunneling, error_is_reported)
{
    stopTunnelingServer();

    whenRequestTunnel();
    thenTunnelIsNotEstablished();
}

TEST_P(HttpTunneling, timeout_supported)
{
    setEstablishTunnelTimeout(std::chrono::milliseconds(1));

    givenSilentTunnellingServer();

    whenRequestTunnel();

    thenTunnelIsNotEstablished();
    andResultCodeIs(SystemError::timedOut);
}

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TEST_CASE_P(
    EveryMethod,
    HttpTunneling,
    ::testing::Values(TunnelMethod::all));

INSTANTIATE_TEST_CASE_P(
    GetPostWithLargeMessageBody,
    HttpTunneling,
    ::testing::Values(TunnelMethod::getPost));

INSTANTIATE_TEST_CASE_P(
    ConnectionUpgrade,
    HttpTunneling,
    ::testing::Values(TunnelMethod::connectionUpgrade));

INSTANTIATE_TEST_CASE_P(
    Experimental,
    HttpTunneling,
    ::testing::Values(TunnelMethod::experimental));

INSTANTIATE_TEST_CASE_P(
    Ssl,
    HttpTunneling,
    ::testing::Values(TunnelMethod::ssl));

} // namespace nx::network::http::tunneling::test
