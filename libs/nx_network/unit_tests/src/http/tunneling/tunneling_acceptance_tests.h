// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/http/tunneling/detail/connection_upgrade_tunnel_client.h>
#include <nx/network/http/tunneling/detail/experimental_tunnel_client.h>
#include <nx/network/http/tunneling/detail/get_post_tunnel_client.h>
#include <nx/network/http/tunneling/detail/ssl_tunnel_client.h>
#include <nx/network/http/tunneling/server.h>
#include <nx/network/nx_network_ini.h>
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
static constexpr char kTunnelTag[] = "/HttpTunnelingTest";

template<typename TunnelMethodMask>
class HttpTunneling:
    public ::testing::Test,
    private TunnelAuthorizer<>
{
public:
    HttpTunneling():
        m_tunnelingServer(std::make_unique<Server<>>(
            [this](auto&&... args) { saveServerTunnel(std::forward<decltype(args)>(args)...); },
            (TunnelAuthorizer<>*) this))
    {
        m_clientFactoryBak = detail::ClientFactory::instance().setCustomFunc(
            [this](
                const std::string& tag,
                const nx::utils::Url& baseUrl,
                std::optional<int> forcedTunnelType,
                const ConnectOptions& options)
            {
                return m_localFactory.create(tag, baseUrl, forcedTunnelType, options);
            });

        enableTunnelMethods(TunnelMethodMask::value);
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
        m_iniTweaks = std::make_unique<nx::kit::IniConfig::Tweaks>();
        m_iniTweaks->set(&nx::network::ini().useDefaultSslCertificateVerificationByOs, false);

        startHttpServer();

        m_tunnelingClient = std::make_unique<Client>(m_baseUrl, kTunnelTag);
        m_tunnelingClient.reset();
    }

    void stopTunnelingServer()
    {
        // NOTE: pleaseStopSync does not guarantee that the server will stop listening.
        // So, destroying the server.
        m_httpServer.reset();

        m_iniTweaks.reset();
    }

    void setEstablishTunnelTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }

    void addDelayToTunnelAuthorization(std::chrono::milliseconds timeout)
    {
        m_tunnelAuthorizationDelay = timeout;
    }

    void setHttpConnectionInactivityTimeout(std::chrono::milliseconds timeout)
    {
        m_httpConnectionInactivityTimeout = timeout;

        if (m_httpServer)
        {
            m_httpServer->server().setConnectionInactivityTimeout(
                *m_httpConnectionInactivityTimeout);
        }
    }

    void givenTunnellingServer()
    {
        m_tunnelingServer->registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());
    }

    void addSomeCustomHttpHeadersToTheClient()
    {
        m_customHeaders.emplace("H1", "V1.1");
        m_customHeaders.emplace("H1", "V1.2");
        m_customHeaders.emplace("H2", "V2");
    }

    void givenDelayingTunnellingServer(std::chrono::milliseconds delay)
    {
        m_httpServer->registerRequestProcessorFunc(
            http::kAnyPath,
            [this, delay](auto&&... args) { respondWithDelay(delay, std::move(args)...); });
    }

    void whenRequestTunnel()
    {
        if (!m_tunnelingClient)
            m_tunnelingClient = std::make_unique<Client>(m_baseUrl, kTunnelTag);

        whenRequestTunnel(m_tunnelingClient.get());
    }

    void whenRequestTunnel(Client* client)
    {
        client->setCustomHeaders(m_customHeaders);

        if (m_timeout)
            client->setTimeout(*m_timeout);
        else
            client->setTimeout(nx::network::kNoTimeout);

        client->openTunnel(
            [this](auto&&... args) { saveClientTunnel(std::move(args)...); });
    }

    void whenCloseClientConnection()
    {
        m_prevClientTunnelResult.connection.reset();
    }

    void blockUntilTunnelAuthorizationIsRequested()
    {
        m_tunnelAuthorizationRequestedEvent.pop();
    }

    void deleteTunnelClient()
    {
        m_tunnelingClient.reset();
    }

    void thenTunnelIsEstablished()
    {
        thenClientTunnelSucceeded();

        thenServerTunnelSucceeded();

        assertDataExchangeWorksThroughTheTunnel();
    }

    void thenClientTunnelSucceeded()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_EQ(ResultCode::ok, m_prevClientTunnelResult.resultCode);
        ASSERT_EQ(SystemError::noError, m_prevClientTunnelResult.sysError);
        ASSERT_TRUE(StatusCode::isSuccessCode(*m_prevClientTunnelResult.httpStatus));
        ASSERT_NE(nullptr, m_prevClientTunnelResult.connection);
    }

    void thenServerTunnelSucceeded()
    {
        // NOTE: Since we have concurrent tunnels now, there can be multiple tunnels established.
        // So, we need to choose the server tunnel that

        do
        {
            m_prevServerTunnelConnection = m_serverTunnels.pop();
            ASSERT_NE(nullptr, m_prevServerTunnelConnection);
        }
        while (m_prevClientTunnelResult.connection
            && m_prevClientTunnelResult.connection->getLocalAddress() !=
                m_prevServerTunnelConnection->getForeignAddress());
    }

    void thenTunnelIsNotEstablished()
    {
        m_prevClientTunnelResult = m_clientTunnels.pop();

        ASSERT_NE(ResultCode::ok, m_prevClientTunnelResult.resultCode);
        // NOTE: sysError and httpStatus of m_prevClientTunnelResult may be ok.
        ASSERT_EQ(nullptr, m_prevClientTunnelResult.connection);
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

    void andResultCodeIs(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_prevClientTunnelResult.sysError);
    }

    Client& tunnelingClient()
    {
        if (!m_tunnelingClient)
            m_tunnelingClient = std::make_unique<Client>(m_baseUrl, kTunnelTag);

        return *m_tunnelingClient;
    }

    detail::ClientFactory& tunnelClientFactory()
    {
        return m_localFactory;
    }

    void andCustomHeadersAreReportedToTheServer()
    {
        for (const auto& header: m_customHeaders)
        {
            bool found = false;

            const auto headerRange =
                m_lastOpenTunnelRequestReceivedByServer.headers.equal_range(header.first);
            for (auto it = headerRange.first; it != headerRange.second; ++it)
            {
                if (it->second == header.second)
                {
                    found = true;
                    break;
                }
            }

            ASSERT_TRUE(found);
        }
    }

    std::promise<void> blockServerTunnelHandler()
    {
        std::promise<void> promise;
        m_continueFuture = promise.get_future();
        return promise;
    }

    void deleteTunnellingServer()
    {
        m_httpServer.reset();
        m_tunnelingServer.reset();
    }

    nx::utils::Url baseUrl() const
    {
        return m_baseUrl;
    }

    void setBaseUrl(const nx::utils::Url& url)
    {
        m_baseUrl = url;
        m_tunnelingClient = std::make_unique<Client>(m_baseUrl, kTunnelTag);
    }

    TestHttpServer& httpServer()
    {
        return *m_httpServer;
    }

    AbstractStreamSocket* lastServerTunnelConnection()
    {
        return m_prevServerTunnelConnection.get();
    }

    Server<>& tunnelingServer()
    {
        return *m_tunnelingServer;
    }

private:
    struct DelayedRequestContext
    {
        aio::Timer timer;

        ~DelayedRequestContext()
        {
            timer.pleaseStopSync();
        }
    };

    std::unique_ptr<nx::kit::IniConfig::Tweaks> m_iniTweaks;
    nx::Mutex m_mutex;
    std::vector<std::unique_ptr<DelayedRequestContext>> m_delayedRequests;
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverTunnels;
    http::Request m_lastOpenTunnelRequestReceivedByServer;
    std::unique_ptr<Server<>> m_tunnelingServer;
    nx::utils::SyncQueue<OpenTunnelResult> m_clientTunnels;
    std::unique_ptr<Client> m_tunnelingClient;
    detail::ClientFactory m_localFactory;
    detail::ClientFactory::Function m_clientFactoryBak;
    http::HttpHeaders m_customHeaders;
    OpenTunnelResult m_prevClientTunnelResult;
    nx::utils::Url m_baseUrl;
    std::unique_ptr<AbstractStreamSocket> m_prevServerTunnelConnection;
    std::optional<std::chrono::milliseconds> m_timeout;
    std::optional<std::chrono::milliseconds> m_httpConnectionInactivityTimeout;
    std::optional<std::chrono::milliseconds> m_tunnelAuthorizationDelay;
    nx::utils::SyncQueue</*dummy*/ int> m_tunnelAuthorizationRequestedEvent;
    std::optional<std::future<void>> m_continueFuture;
    std::unique_ptr<TestHttpServer> m_httpServer;

    virtual void authorize(
        const RequestContext* requestContext,
        CompletionHandler completionHandler) override
    {
        m_tunnelAuthorizationRequestedEvent.push(/*dummy*/ 0);

        if (m_tunnelAuthorizationDelay)
            std::this_thread::sleep_for(*m_tunnelAuthorizationDelay);

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_lastOpenTunnelRequestReceivedByServer = requestContext->request;
        }

        completionHandler(StatusCode::ok, {});
    }

    void startHttpServer()
    {
        m_httpServer = std::make_unique<TestHttpServer>();
        if (m_httpConnectionInactivityTimeout)
        {
            m_httpServer->server().setConnectionInactivityTimeout(
                *m_httpConnectionInactivityTimeout);
        }

        ASSERT_TRUE(m_httpServer->bindAndListen());
        m_baseUrl = nx::network::url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath);
    }

    void enableTunnelMethods(int tunnelMethodMask)
    {
        m_localFactory.clear();

        if (tunnelMethodMask & TunnelMethod::getPost)
            m_localFactory.registerClientType<detail::GetPostTunnelClient>(0);

        if (tunnelMethodMask & TunnelMethod::connectionUpgrade)
            m_localFactory.registerClientType<detail::ConnectionUpgradeTunnelClient>(1);

        if (tunnelMethodMask & TunnelMethod::experimental)
            m_localFactory.registerClientType<detail::ExperimentalTunnelClient>(2);

        if (tunnelMethodMask & TunnelMethod::ssl)
            m_localFactory.registerClientType<detail::SslTunnelClient>(3);
    }

    void saveClientTunnel(OpenTunnelResult result)
    {
        m_clientTunnels.push(std::move(result));
    }

    void saveServerTunnel(std::unique_ptr<AbstractStreamSocket> connection)
    {
        if (m_continueFuture)
            m_continueFuture->wait();

        m_serverTunnels.push(std::move(connection));
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

    void respondWithDelay(
        std::chrono::milliseconds delay,
        http::RequestContext /*requestContext*/,
        http::RequestProcessedHandler handler)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto ctx = std::make_unique<DelayedRequestContext>();
        m_delayedRequests.push_back(std::move(ctx));
        m_delayedRequests.back()->timer.start(
            delay,
            [handler = std::move(handler)]() { handler(StatusCode::ok); });
    }
};

TYPED_TEST_SUITE_P(HttpTunneling);

} // namespace nx::network::http::tunneling::test
