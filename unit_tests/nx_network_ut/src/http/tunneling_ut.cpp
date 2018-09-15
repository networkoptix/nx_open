#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/http/tunneling/server.h>
#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/http/tunneling/detail/get_post_tunnel_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::tunneling::test {

enum TunnelMethod
{
    getPost = 1,
    connectionUpgrade = 2,
    all = getPost | connectionUpgrade,
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
        m_httpServer = std::make_unique<TestHttpServer>();

        m_tunnelingServer.registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::network::url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath);
    }

    void stopTunnelingServer()
    {
        m_httpServer->server().pleaseStopSync();
    }

    void whenRequestTunnel()
    {
        m_tunnelingClient = std::make_unique<Client>(m_baseUrl);

        m_tunnelingClient->openTunnel(
            std::bind(&HttpTunneling::saveClientTunnel, this, std::placeholders::_1));
    }

    void thenTunnelIsEstablished()
    {
        thenClientTunnelSucceeded();

        ASSERT_NE(nullptr, m_serverTunnels.pop());
        // TODO: Exchange some data between client & server connections.
    }

    void thenClientTunnelSucceeded()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_EQ(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_TRUE(StatusCode::isSuccessCode(m_prevClientTunnelResult.httpStatus));
        ASSERT_NE(nullptr, m_prevClientTunnelResult.connection);
    }

    void thenTunnelIsNotEstablished()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();
        
        ASSERT_NE(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_EQ(nullptr, m_prevClientTunnelResult.connection);
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

    void enableTunnelMethods(int tunnelMethodMask)
    {
        m_localFactory.clear();

        if (tunnelMethodMask & TunnelMethod::getPost)
            m_localFactory.registerClientType<detail::GetPostTunnelClient>();

        //if (tunnelMethodMask & TunnelMethod::connectionUpgrade)
        //    m_localFactory.registerClientType<detail::ConnectionUpgradeTunnelClient>();
    }

    void saveClientTunnel(OpenTunnelResult result)
    {
        m_clientTunnels.push(std::move(result));
    }

    void saveServerTunnel(std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_serverTunnels.push(std::move(connection));
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

INSTANTIATE_TEST_CASE_P(
    EveryMethodIsEnabled,
    HttpTunneling,
    ::testing::Values(TunnelMethod::all));

} // namespace nx::network::http::tunneling::test
