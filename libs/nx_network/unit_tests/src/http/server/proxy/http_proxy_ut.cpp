// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <functional>
#include <set>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/connection_cache.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/global_context.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/network/http/server/proxy/proxy_worker.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/network/test_support/http_resource_storage.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/time.h>

namespace nx::network::http::server::proxy::test {

class IncompleteBodySource:
    public BufferSource
{
    using base_type = BufferSource;

public:
    using base_type::base_type;

    virtual std::optional<uint64_t> contentLength() const override
    {
        return *base_type::contentLength() * 2;
    }
};

//-------------------------------------------------------------------------------------------------

class BodyWithoutContentLengthSource:
    public BufferSource
{
    using base_type = BufferSource;

public:
    using base_type::base_type;

    virtual std::optional<uint64_t> contentLength() const override
    {
        return std::nullopt;
    }
};

//-------------------------------------------------------------------------------------------------

class AuditedResourceStorage: public http::test::ResourceStorage
{
public:
    void setAuditFunc(std::function<void(const RequestContext&)> func)
    {
        m_func = std::move(func);
    }

    virtual void registerHttpHandlers(
        AbstractMessageDispatcher* dispatcher,
        const std::string& basePath) override
    {
        setBasePath(basePath);

        dispatcher->registerRequestProcessorFunc(
            Method::post,
            kAnyPath,
            [this](RequestContext requestContext, RequestProcessedHandler handler)
            {
                if (m_func)
                    m_func(requestContext);
                postResource(std::move(requestContext), std::move(handler));
            });

        dispatcher->registerRequestProcessorFunc(
            Method::put,
            kAnyPath,
            [this](RequestContext requestContext, RequestProcessedHandler handler)
            {
                if (m_func)
                    m_func(requestContext);
                postResource(std::move(requestContext), std::move(handler));
            });

        dispatcher->registerRequestProcessorFunc(
            Method::get,
            kAnyPath,
            [this](RequestContext requestContext, RequestProcessedHandler handler)
            {
                if (m_func)
                    m_func(requestContext);
                getResource(std::move(requestContext), std::move(handler));
            });

        dispatcher->registerRequestProcessorFunc(
            Method::delete_,
            kAnyPath,
            [this](RequestContext requestContext, RequestProcessedHandler handler)
            {
                if (m_func)
                    m_func(requestContext);
                deleteResource(std::move(requestContext), std::move(handler));
            });
    }

private:
    std::function<void(const RequestContext&)> m_func;
};

//-------------------------------------------------------------------------------------------------

static constexpr char kResourcePath[] = "/resource";
static constexpr char kResourceWithoutContentLengthPath[] = "/resource-2";
static constexpr char kSaveStreamingRequestBodyPath[] = "/save-streaming-request-body";

class HttpProxy:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_socketReusesCount = 0;
        m_socketCreationsCount = 0;
        startTargetServer();
        startProxyServer();
    }

    virtual void TearDown() override
    {
        m_targetServer.reset();
        m_proxyServer.reset();
    }

    void whenRequestResource()
    {
        downloadResource(kResourcePath);
    }

    void whenRequestResourceWithoutContentLength()
    {
        downloadResource(kResourceWithoutContentLengthPath);
    }

    void whenPostResource()
    {
        m_requestBodySent = nx::utils::generateRandomName(16 * 1024);

        auto client = prepareHttpClient();
        ASSERT_TRUE(client->doPost(requestUrl(kResourcePath), "text/plain", m_requestBodySent));
    }

    void whenDeleteResource()
    {
        auto client = prepareHttpClient();
        ASSERT_TRUE(client->doDelete(requestUrl(kResourcePath)));
    }

    void whenSendRequestWithIncompleteBody()
    {
        m_requestBodySent = nx::utils::generateRandomName(16*1024);

        auto client = prepareHttpClient();
        ASSERT_TRUE(client->doPost(
            requestUrl(kSaveStreamingRequestBodyPath),
            std::make_unique<IncompleteBodySource>(
                "application/octet-stream",
                m_requestBodySent)));
    }

    void thenTheResourceIsDownloaded()
    {
        ASSERT_EQ(*m_resourceStorage.get(m_lastRequestedResourcePath), m_requestBodyReceived);
    }

    void thenTheResourceIsUploaded()
    {
        ASSERT_EQ(m_requestBodySent, *m_resourceStorage.get(kResourcePath));
    }

    void thenTheResourceIsDeleted()
    {
        ASSERT_FALSE(m_resourceStorage.get(kResourcePath));
    }

    void thenTheRequestBodySentIsDeliveredToTheTarget()
    {
        while (requestBodyReceived() != requestBodySent())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        m_requestBodyReceived.clear();
    }

    void thenConnectionIsReused()
    {
        EXPECT_EQ(1, m_socketCreationsCount);
        EXPECT_EQ(1, m_socketReusesCount);
    }

    void thenConnectionIsNotReused()
    {
        EXPECT_EQ(2, m_socketCreationsCount);
        EXPECT_EQ(0, m_socketReusesCount);
    }

    TestHttpServer& targetServer()
    {
        return *m_targetServer;
    }

    nx::utils::Url requestUrl(const std::string& path)
    {
        return url::Builder()
            .setScheme(kUrlSchemeName)
            .setEndpoint(m_targetServer->serverAddress())
            .setPath(path);
    }

    std::unique_ptr<HttpClient> prepareHttpClient()
    {
        auto client = std::make_unique<HttpClient>(ssl::kAcceptAnyCertificate);
        client->setResponseReadTimeout(kNoTimeout);
        client->setProxyVia(m_proxyServer->serverAddress(),
            /*isSecure*/ false, nx::network::ssl::kAcceptAnyCertificate);
        if (m_credentials)
            client->setCredentials(*m_credentials);
        return client;
    }

    void enableAuthenticationOnTarget()
    {
        m_credentials = Credentials(
            nx::utils::generateRandomName(7),
            PasswordAuthToken(nx::utils::generateRandomName(7)));

        m_targetServer->enableAuthentication(".*");
        m_targetServer->registerUserCredentials(*m_credentials);
    }

    void restartTargetServer()
    {
        auto addr = m_targetServer->serverAddress();
        m_targetServer.reset();
        startTargetServer(addr);
    }

private:
    AuditedResourceStorage m_resourceStorage;
    nx::Mutex m_mutex;
    nx::Buffer m_requestBodyReceived;
    nx::Buffer m_requestBodySent;
    std::optional<Credentials> m_credentials;
    std::string m_lastRequestedResourcePath;
    std::unique_ptr<TestHttpServer> m_proxyServer;
    std::unique_ptr<TestHttpServer> m_targetServer;
    nx::Mutex m_activeSocketsMutex;
    std::set<AbstractStreamSocket*> m_activeSockets;
    unsigned int m_socketReusesCount;
    unsigned int m_socketCreationsCount;

    void startTargetServer(const SocketAddress& endpoint = SocketAddress::anyPrivateAddress)
    {
        m_targetServer = std::make_unique<TestHttpServer>();
        m_targetServer->registerRequestProcessorFunc(
            kSaveStreamingRequestBodyPath,
            [this](auto&&... args) { saveStreamingRequestBody(std::forward<decltype(args)>(args)...); },
            Method::post,
            MessageBodyDeliveryType::stream);

        m_targetServer->registerRequestProcessorFunc(
            kResourceWithoutContentLengthPath,
            [this](auto&&... args)
            {
                sendResourceWithoutContentLength(std::forward<decltype(args)>(args)...);
            },
            Method::get);

        m_resourceStorage.setAuditFunc(
            [this](const RequestContext& requestContext)
            {
                registerSocket(requestContext.conn.lock()->socket().get());
            });

        m_resourceStorage.registerHttpHandlers(
            &m_targetServer->httpMessageDispatcher(),
            "/");

        m_resourceStorage.save(kResourcePath, nx::utils::generateRandomName(4 * 1024));

        m_resourceStorage.save(
            kResourceWithoutContentLengthPath,
            nx::utils::generateRandomName(4*1024));

        ASSERT_TRUE(m_targetServer->bindAndListen(endpoint));
    }

    void startProxyServer()
    {
        m_proxyServer = std::make_unique<TestHttpServer>();
        m_proxyServer->registerRequestProcessor<ProxyHandler>(kAnyPath);

        ASSERT_TRUE(m_proxyServer->bindAndListen());
    }

    void registerSocket(AbstractStreamSocket* sock)
    {
        NX_MUTEX_LOCKER lk(&m_activeSocketsMutex);
        auto result = m_activeSockets.insert(sock);
        if (result.second)
        {
            sock->setBeforeDestroyCallback(
                [this, sock]
                {
                    NX_MUTEX_LOCKER lk(&m_activeSocketsMutex);
                    m_activeSockets.erase(sock);
                });
            ++m_socketCreationsCount;
        }
        else
        {
            ++m_socketReusesCount;
        }
    }

    void saveStreamingRequestBody(
        RequestContext requestContext,
        RequestProcessedHandler handler)
    {
        saveBody(std::move(requestContext.body));

        handler(StatusCode::ok);

        registerSocket(requestContext.conn.lock()->socket().get());
    }

    void sendResourceWithoutContentLength(
        RequestContext requestContext,
        RequestProcessedHandler handler)
    {
        const auto resource = m_resourceStorage.get(
            requestContext.request.requestLine.url.path().toStdString());
        if (!resource)
            return handler(StatusCode::notFound);

        RequestResult result(StatusCode::ok);
        result.body = std::make_unique<BodyWithoutContentLengthSource>("text/plain", *resource);

        handler(std::move(result));

        registerSocket(requestContext.conn.lock()->socket().get());
    }

    void saveBody(std::unique_ptr<AbstractMsgBodySourceWithCache> body)
    {
        auto bodyPtr = body.get();
        bodyPtr->readAsync(
            [this, body = std::move(body)](
                SystemError::ErrorCode resultCode, nx::Buffer data) mutable
            {
                auto bodyLocal = std::move(body);

                NX_MUTEX_LOCKER lock(&m_mutex);

                if (resultCode == SystemError::noError && data.size() > 0)
                    saveBody(std::move(bodyLocal));
                m_requestBodyReceived += std::move(data);
            });
    }

    nx::Buffer requestBodyReceived()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_requestBodyReceived;
    }


    nx::Buffer requestBodySent()
    {
        return m_requestBodySent;
    }

    void downloadResource(const char* resourcePath)
    {
        auto client = prepareHttpClient();
        ASSERT_TRUE(client->doGet(requestUrl(resourcePath)));
        if (auto body = client->fetchEntireMessageBody())
            m_requestBodyReceived = std::move(*body);

        m_lastRequestedResourcePath = resourcePath;
    }
};

TEST_F(HttpProxy, target_response_with_body_is_proxied)
{
    whenRequestResource();
    thenTheResourceIsDownloaded();
}

TEST_F(HttpProxy, request_with_body_is_proxied)
{
    whenPostResource();
    thenTheResourceIsUploaded();
}

TEST_F(HttpProxy, request_without_body_is_proxied)
{
    whenDeleteResource();
    thenTheResourceIsDeleted();
}

TEST_F(HttpProxy, request_body_is_streamed_to_the_target)
{
    whenSendRequestWithIncompleteBody();
    thenTheRequestBodySentIsDeliveredToTheTarget();
}

TEST_F(HttpProxy, client_connection_is_closed_by_proxy_to_signal_eof)
{
    whenRequestResourceWithoutContentLength();
    thenTheResourceIsDownloaded();
}

//-------------------------------------------------------------------------------------------------

static constexpr char kUpgradeConnectionPath[] = "/connection-upgrade";
static constexpr char kUpgradeProtocol[] = "TEST/0.1";

class HttpProxyConnectionUpgrade:
    public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        targetServer().registerRequestProcessor(
            kUpgradeConnectionPath,
            [this]()
            {
                return std::make_unique<handler::CreateTunnelHandler>(
                    kUpgradeProtocol,
                    [this](auto&&... args) { saveServerTunnel(std::forward<decltype(args)>(args)...); });
            });
    }

    void givenUpgradedConnection()
    {
        whenUpgradeConnection();

        thenRequestIsDeliveredToTheTargetServer();
        thenUpgradeSucceeded();
    }

    void whenUpgradeConnection()
    {
        auto httpClient = prepareHttpClient();
        ASSERT_TRUE(httpClient->doUpgrade(requestUrl(kUpgradeConnectionPath), kUpgradeProtocol));
        m_clientTunnels.push(httpClient->takeSocket());
    }

    void thenRequestIsDeliveredToTheTargetServer()
    {
        m_serverConnection = m_serverTunnels.pop();
        ASSERT_NE(nullptr, m_serverConnection);
    }

    void thenUpgradeSucceeded()
    {
        m_clientConnection = m_clientTunnels.pop();
        ASSERT_NE(nullptr, m_clientConnection);
    }

    void whenUploadSomeData()
    {
        m_uploadedData = nx::utils::generateRandomName(16 * 1024);

        ASSERT_EQ(
            m_uploadedData.size(),
            m_clientConnection->send(m_uploadedData.data(), m_uploadedData.size()));
    }

    void whenPushingSomeDataFromServer()
    {
        m_dataPushedByServer = nx::utils::generateRandomName(16 * 1024);

        ASSERT_EQ(
            m_dataPushedByServer.size(),
            m_serverConnection->send(m_dataPushedByServer.data(), m_dataPushedByServer.size()));
    }

    void thenUploadedDataIsReceivedByServer()
    {
        assertConnectionReceivedData(m_serverConnection.get(), m_uploadedData);
    }

    void thenDataIsDownloadedByClient()
    {
        assertConnectionReceivedData(m_clientConnection.get(), m_dataPushedByServer);
    }

    void assertUploadingDataWorks()
    {
        whenUploadSomeData();
        thenUploadedDataIsReceivedByServer();
    }

    void assertDownloadingDataWorks()
    {
        whenPushingSomeDataFromServer();
        thenDataIsDownloadedByClient();
    }

private:
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_clientTunnels;
    std::unique_ptr<AbstractStreamSocket> m_clientConnection;
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>> m_serverTunnels;
    std::unique_ptr<AbstractStreamSocket> m_serverConnection;
    nx::Buffer m_uploadedData;
    nx::Buffer m_dataPushedByServer;

    void saveServerTunnel(
        std::unique_ptr<AbstractStreamSocket> connection,
        RequestPathParams /*requestParams*/)
    {
        if (connection)
            connection->setNonBlockingMode(false);
        m_serverTunnels.push(std::move(connection));
    }

    void assertConnectionReceivedData(
        AbstractCommunicatingSocket* connection,
        const nx::Buffer& expected)
    {
        nx::Buffer buf;
        buf.resize(expected.size());

        ASSERT_EQ(buf.size(), connection->recv(buf.data(), buf.size(), MSG_WAITALL));
        ASSERT_EQ(expected, buf);
    }
};

TEST_F(HttpProxyConnectionUpgrade, upgrade_messages_are_forwarded)
{
    whenUpgradeConnection();

    thenRequestIsDeliveredToTheTargetServer();
    thenUpgradeSucceeded();
}

TEST_F(HttpProxyConnectionUpgrade, upgraded_connection_traffic_is_proxied_in_both_directions)
{
    givenUpgradedConnection();

    assertUploadingDataWorks();
    assertDownloadingDataWorks();
}

TEST_F(HttpProxyConnectionUpgrade, upgrade_with_authentication_works)
{
    enableAuthenticationOnTarget();

    givenUpgradedConnection();

    assertUploadingDataWorks();
    assertDownloadingDataWorks();
}

//-------------------------------------------------------------------------------------------------

class HttpProxyConnectionCache: public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();
        SocketGlobals::httpGlobalContext().connectionCache = nx::network::ConnectionCache();
    }

    virtual void TearDown() override
    {
        base_type::TearDown();
        SocketGlobals::httpGlobalContext().connectionCache = nx::network::ConnectionCache();
    }

    void whenGetFromSameDestinationTwiceInSequence()
    {
        whenRequestResource();
        thenTheResourceIsDownloaded();
        whenRequestResource();
        thenTheResourceIsDownloaded();
    }

    void whenPostToSameDestinationTwiceInSequence()
    {
        whenPostResource();
        thenTheResourceIsUploaded();
        whenPostResource();
        thenTheResourceIsUploaded();
    }

    void whenDeleteFromSameDestinationTwiceInSequence()
    {
        whenDeleteResource();
        thenTheResourceIsDeleted();
        whenDeleteResource();
        thenTheResourceIsDeleted();
    }
};

TEST_F(HttpProxyConnectionCache,
    connection_to_the_proxy_target_after_response_with_body_is_reused)
{
    whenGetFromSameDestinationTwiceInSequence();
    thenConnectionIsReused();
}

TEST_F(HttpProxyConnectionCache,
    connection_to_the_proxy_target_after_request_with_body_is_reused)
{
    whenPostToSameDestinationTwiceInSequence();
    thenConnectionIsReused();
}

TEST_F(HttpProxyConnectionCache,
    connection_to_the_proxy_target_after_request_without_body_is_reused)
{
    whenDeleteFromSameDestinationTwiceInSequence();
    thenConnectionIsReused();
}

TEST_F(HttpProxyConnectionCache,
    connection_to_the_proxy_target_after_closing_by_it_is_not_reused)
{
    whenRequestResourceWithoutContentLength();
    thenTheResourceIsDownloaded();
    whenRequestResourceWithoutContentLength();
    thenTheResourceIsDownloaded();
    thenConnectionIsNotReused();
}

TEST_F(HttpProxyConnectionCache,
    cached_connection_to_the_proxy_target_may_close_unexpectedly)
{
    whenRequestResource();
    thenTheResourceIsDownloaded();
    restartTargetServer();
    whenRequestResource();
    thenTheResourceIsDownloaded();
    thenConnectionIsNotReused();
}

TEST_F(HttpProxyConnectionCache, connection_to_the_proxy_target_is_not_reused_after_expiration)
{
    nx::utils::test::ScopedSyntheticMonotonicTime scopedSyntheticMonotonicTime;
    whenRequestResource();
    thenTheResourceIsDownloaded();
    // Adding a connection to the cache is an asynchronous operation. Thus we need to wait for
    // its completion before shifting the time.
    while (SocketGlobals::httpGlobalContext().connectionCache.size() != 1)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    scopedSyntheticMonotonicTime.applyRelativeShift(std::chrono::minutes(2));
    whenRequestResource();
    thenTheResourceIsDownloaded();
    thenConnectionIsNotReused();
}

} // namespace nx::network::http::server::proxy::test
