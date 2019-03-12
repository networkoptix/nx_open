#include <vector>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/m3u/m3u_playlist.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

template<typename Acceptor>
class AcceptorToServerSocketAdapter:
    public nx::network::StreamServerSocketDelegate
{
    using base_type = nx::network::StreamServerSocketDelegate;

public:
    AcceptorToServerSocketAdapter(std::unique_ptr<Acceptor> acceptor):
        base_type(nullptr),
        m_acceptor(std::move(acceptor))
    {
    }

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override
    {
        return m_acceptor->getAioThread();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        m_acceptor->bindToAioThread(aioThread);
    }

    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->post(std::move(handler));
    }

    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->dispatch(std::move(handler));
    }

    virtual void pleaseStopSync() override
    {
        m_acceptor->pleaseStopSync();
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->pleaseStop(std::move(handler));
    }

    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        *millis = network::kNoTimeout.count();
        return true;
    }

    virtual void acceptAsync(network::AcceptCompletionHandler handler) override
    {
        m_acceptor->acceptAsync(std::move(handler));
    }

protected:
    virtual void cancelIoInAioThread() override
    {
        m_acceptor->cancelIOSync();
    }

private:
    std::unique_ptr<Acceptor> m_acceptor;
};

using RelayConnectionAcceptor = nx::network::cloud::relay::ConnectionAcceptor;

constexpr char kTestPath[] = "/HttpProxy";
constexpr char kTestMessage[] = "Hello, world";
constexpr char kTestMessageMimeType[] = "text/plain";

class HttpProxy:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    HttpProxy(BasicComponentTest::Mode mode = BasicComponentTest::Mode::cluster):
        BasicComponentTest(mode)
    {
    }

    ~HttpProxy()
    {
        m_peerServer.reset();

        decltype(m_httpClients) httpClients;
        {
            QnMutexLocker lock(&m_mutex);
            httpClients = std::exchange(m_httpClients, {});
        }

        for (auto& httpClient: httpClients)
        {
            httpClient->pleaseStopSync();
            httpClient.reset();
        }

        for (const auto& hostName: m_registeredHostNames)
            nx::network::SocketGlobals::addressResolver().removeFixedAddress(hostName);
    }

protected:
    virtual void SetUp() override
    {
        addRelayInstance();
    }

    virtual nx::utils::Url proxyUrlForHost(
        const Relay& relayInstance,
        const std::string& hostName)
    {
        auto url = relayInstance.basicUrl();

        auto host = lm("%1.%2").args(hostName, url.host()).toStdString();
        addLocalHostAlias(host);
        url.setHost(host.c_str());
        url.setPath(kTestPath);

        return url;
    }

    void givenListeningPeer(
        network::ssl::EncryptionUse encryptionUse = network::ssl::EncryptionUse::never)
    {
        if (m_listeningPeerHostName.empty())
        {
            m_listeningPeerHostName =
                QnUuid::createUuid().toSimpleByteArray().toStdString();
        }

        m_peerServer = addListeningPeer(
            m_listeningPeerHostName,
            kTestMessage,
            encryptionUse);
    }

    void givenListeningPeerWithCompositeName()
    {
        m_listeningPeerHostName =
            QnUuid::createUuid().toSimpleByteArray().toStdString() +
            "." +
            QnUuid::createUuid().toSimpleByteArray().toStdString();

        givenListeningPeer();
    }

    void whenSendHttpRequestToPeer()
    {
        sendHttpRequestToPeer(m_listeningPeerHostName, relay());
    }

    void whenSendHttpRequestToUnknownPeer()
    {
        sendHttpRequestToPeer(
            nx::utils::generateRandomName(7).toStdString(),
            relay());
    }

    void whenSendOptionsRequestToPeer()
    {
        network::http::HttpHeaders headers;
        headers.emplace("Origin", "hz");
        headers.emplace("Access-Control-Request-Headers", "Header, Header-Header");

        sendHttpRequestToPeer(
            m_listeningPeerHostName,
            relay(),
            network::http::Method::options,
            std::move(headers));
    }

    void sendHttpRequestToPeer(
        const std::string& hostName,
        const Relay& relayInstance,
        network::http::Method::ValueType method = network::http::Method::get,
        network::http::HttpHeaders headers = {})
    {
        sendHttpRequestToPeer(
            proxyUrlForHost(relayInstance, hostName),
            method,
            headers);
    }

    void sendHttpRequestToPeer(
        const nx::utils::Url& url,
        network::http::Method::ValueType method = network::http::Method::get,
        network::http::HttpHeaders headers = {})
    {
        auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
        httpClient->setAdditionalHeaders(std::move(headers));
        httpClient->setSendTimeout(network::kNoTimeout);
        httpClient->setResponseReadTimeout(network::kNoTimeout);
        httpClient->setMessageBodyReadTimeout(network::kNoTimeout);

        httpClient->executeInAioThreadSync(
            [this, &httpClient, method, url]()
            {
                httpClient->doRequest(
                    method,
                    url,
                    std::bind(&HttpProxy::saveResponse, this, httpClient.get()));

                m_lastRequest = httpClient->request();
            });

        QnMutexLocker lock(&m_mutex);
        m_httpClients.push_back(std::move(httpClient));
    }

    void thenResponseIsReceived()
    {
        m_lastResponse = m_httpResponseQueue.pop();
        ASSERT_NE(nullptr, m_lastResponse);
    }

    void thenResponseFromPeerIsReceived()
    {
        thenResponseIsReceived();
        andResponseStatusCodeIs(nx::network::http::StatusCode::ok);
        addMessageBodyIs(kTestMessage);
    }

    void thenSuccessOptionsResponseIsReceived()
    {
        thenResponseIsReceived();
        andResponseStatusCodeIs(nx::network::http::StatusCode::ok);

        assertLastResponseIsACorrectCorsOptionsResponse();
    }

    void andRequestWasNotProxiedToThePeer()
    {
        ASSERT_EQ(0, m_requestsServedByPeerCount);
    }

    void andResponseStatusCodeIs(nx::network::http::StatusCode::Value expected)
    {
        ASSERT_EQ(expected, m_lastResponse->statusLine.statusCode);
    }

    void addMessageBodyIs(const nx::Buffer& expectedMessageBody)
    {
        ASSERT_EQ(expectedMessageBody, m_lastResponse->messageBody);
    }

    const std::string& listeningPeerHostName() const
    {
        return m_listeningPeerHostName;
    }

    const nx::network::http::Response& lastResponse() const
    {
        return *m_lastResponse;
    }

    std::unique_ptr<nx::network::http::TestHttpServer> addListeningPeer(
        const std::string& listeningPeerHostName,
        const nx::Buffer& messageBody,
        network::ssl::EncryptionUse encryptionUse = network::ssl::EncryptionUse::never)
    {
        using namespace std::placeholders;

        auto url = relay().basicUrl();
        url.setUserName(listeningPeerHostName.c_str());

        auto acceptor = std::make_unique<RelayConnectionAcceptor>(url);

        std::unique_ptr<nx::network::http::TestHttpServer> peerServer;
        if (encryptionUse == network::ssl::EncryptionUse::always)
        {
            auto sslAcceptor = std::make_unique<network::ssl::StreamServerSocket>(
                std::make_unique<AcceptorToServerSocketAdapter<RelayConnectionAcceptor>>(
                    std::move(acceptor)),
                network::ssl::EncryptionUse::always);
            sslAcceptor->setNonBlockingMode(true);

            peerServer = std::make_unique<nx::network::http::TestHttpServer>(
                std::move(sslAcceptor));
        }
        else if (encryptionUse == network::ssl::EncryptionUse::never)
        {
            peerServer = std::make_unique<nx::network::http::TestHttpServer>(
                std::move(acceptor));
        }

        peerServer->registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpProxy::processHttpRequest, this, _1, _2, messageBody),
            nx::network::http::Method::get);
        peerServer->server().start();

        while (!peerInformationSynchronizedInCluster(listeningPeerHostName))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        return peerServer;
    }

    nx::network::http::Request& lastRequestOnTargetServer()
    {
        if (!m_lastRequestOnTargetServer)
            m_lastRequestOnTargetServer = m_requestOnTargetServerQueue.pop();
        return *m_lastRequestOnTargetServer;
    }

    nx::network::http::Response& lastResponse()
    {
        return *m_lastResponse;
    }

    void addLocalHostAlias(const std::string& alias)
    {
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            alias, network::SocketAddress(network::HostAddress::localhost, 0));
        m_registeredHostNames.push_back(alias);
    }

    nx::network::http::TestHttpServer& listeningPeer()
    {
        return *m_peerServer;
    }

private:
    std::string m_listeningPeerHostName;
    std::unique_ptr<nx::network::http::TestHttpServer> m_peerServer;
    mutable nx::utils::Mutex m_mutex;
    std::vector<std::unique_ptr<nx::network::http::AsyncClient>> m_httpClients;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>>
        m_httpResponseQueue;
    std::vector<std::string> m_registeredHostNames;
    std::unique_ptr<nx::network::http::Response> m_lastResponse;
    network::http::Request m_lastRequest;
    int m_requestsServedByPeerCount = 0;
    std::optional<network::http::Request> m_lastRequestOnTargetServer;
    nx::utils::SyncQueue<network::http::Request> m_requestOnTargetServerQueue;

    void saveResponse(nx::network::http::AsyncClient* httpClient)
    {
        if (httpClient->response())
        {
            auto response = std::make_unique<nx::network::http::Response>(
                *httpClient->response());
            response->messageBody = httpClient->fetchMessageBodyBuffer();
            m_httpResponseQueue.push(std::move(response));
        }
        else
        {
            m_httpResponseQueue.push(nullptr);
        }

        QnMutexLocker lock(&m_mutex);

        m_httpClients.erase(
            std::remove_if(m_httpClients.begin(), m_httpClients.end(),
                [httpClient](const auto& val) { return httpClient == val.get(); }),
            m_httpClients.end());
    }

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler,
        const nx::Buffer& messageBody)
    {
        ++m_requestsServedByPeerCount;

        m_requestOnTargetServerQueue.push(requestContext.request);

        network::http::RequestResult requestResult(network::http::StatusCode::ok);
        requestResult.dataSource =
            std::make_unique<network::http::BufferSource>(kTestMessageMimeType, messageBody);
        completionHandler(std::move(requestResult));
    }

    void assertLastResponseIsACorrectCorsOptionsResponse()
    {
        const auto origin = network::http::getHeaderValue(
            m_lastRequest.headers, "Origin");
        const auto allowedOrigin = network::http::getHeaderValue(
            m_lastResponse->headers, "Access-Control-Allow-Origin");
        ASSERT_EQ(allowedOrigin, origin);

        const auto requestedHeaders = network::http::getHeaderValue(
            m_lastRequest.headers, "Access-Control-Request-Headers");
        const auto allowedHeaders = network::http::getHeaderValue(
            m_lastResponse->headers, "Access-Control-Allow-Headers");
        ASSERT_EQ(allowedHeaders, requestedHeaders);

        const auto requestedMethod = network::http::getHeaderValue(
            m_lastRequest.headers, "Access-Control-Request-Method");
        const auto allowedMethods = network::http::getHeaderValue(
            m_lastResponse->headers, "Access-Control-Allow-Methods");
        ASSERT_TRUE(allowedMethods.indexOf(requestedMethod) != -1);
    }
};

TEST_F(HttpProxy, request_is_passed_to_listening_peer)
{
    givenListeningPeer();
    whenSendHttpRequestToPeer();
    thenResponseFromPeerIsReceived();
}

TEST_F(HttpProxy, proxy_to_unknown_host_produces_bad_gateway_error)
{
    givenListeningPeer();
    whenSendHttpRequestToUnknownPeer();

    thenResponseIsReceived();
    andResponseStatusCodeIs(nx::network::http::StatusCode::badGateway);
}

TEST_F(HttpProxy, works_for_peer_with_composite_name)
{
    givenListeningPeerWithCompositeName();
    whenSendHttpRequestToPeer();
    thenResponseFromPeerIsReceived();
}

TEST_F(HttpProxy, options_request_is_not_proxied_but_processed)
{
    givenListeningPeer();

    whenSendOptionsRequestToPeer();

    thenSuccessOptionsResponseIsReceived();
    andRequestWasNotProxiedToThePeer();
}

//-------------------------------------------------------------------------------------------------

class HttpProxyMultipleServersUnderSameDomain:
    public HttpProxy
{
    using base_type = HttpProxy;

public:
    HttpProxyMultipleServersUnderSameDomain():
        m_domainName(nx::utils::generateRandomName(7).toStdString())
    {
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();
    }

    void givenMultipleListeningPeersUnderSingleDomain()
    {
        constexpr int peerCount = 3;
        for (int i = 0; i < peerCount; ++i)
        {
            m_listeningPeers.push_back(addListeningPeer(
                lm("%1.%2").args(
                    nx::utils::generateRandomName(7).toStdString(),
                    m_domainName).toStdString(),
                lm("%1 from peer %2").args(kTestMessage, i).toUtf8()));
        }
    }

    void givenHostnameFromProxiedDomainRequest()
    {
        whenSendRequestToDomain();
        thenRequestIsDeliveredToAnyServer();
        m_lastResponseMessageBody = lastResponse().messageBody;

        m_lastRequestOnTargetServerHostHeader =
            network::http::getHeaderValue(lastRequestOnTargetServer().headers, "Host").toStdString();
    }

    void whenSendRequestToDomain()
    {
        sendHttpRequestToPeer(m_domainName, relay());
    }

    void whenSendRequestToUniqueServerHostname()
    {
        const auto url = nx::network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(m_lastRequestOnTargetServerHostHeader)
            .setPath(kTestPath).toUrl();
        addLocalHostAlias(url.host().toStdString());
        sendHttpRequestToPeer(url);
    }

    void thenRequestIsDeliveredToAnyServer()
    {
        thenResponseIsReceived();
        andResponseStatusCodeIs(nx::network::http::StatusCode::ok);

        ASSERT_TRUE(lastResponse().messageBody.startsWith(kTestMessage));
    }

    void thenRequestIsDeliveredToTheSameServer()
    {
        thenRequestIsDeliveredToAnyServer();
        ASSERT_EQ(m_lastResponseMessageBody, lastResponse().messageBody);
    }

    void andProxiedRequestHostHeaderContainsUniqueServerHostname()
    {
        const auto hostHeader =
            network::http::getHeaderValue(lastRequestOnTargetServer().headers, "Host");

        const auto url = proxyUrlForHost(relay(), m_domainName);

        ASSERT_NE(nx::network::url::getEndpoint(url), hostHeader);
    }

private:
    std::string m_domainName;
    std::string m_lastRequestOnTargetServerHostHeader;
    nx::Buffer m_lastResponseMessageBody;
    std::vector<std::unique_ptr<nx::network::http::TestHttpServer>> m_listeningPeers;
};

TEST_F(
    HttpProxyMultipleServersUnderSameDomain,
    proxied_request_contains_host_unique_for_the_server)
{
    givenMultipleListeningPeersUnderSingleDomain();

    whenSendRequestToDomain();

    thenRequestIsDeliveredToAnyServer();
    andProxiedRequestHostHeaderContainsUniqueServerHostname();
}

TEST_F(
    HttpProxyMultipleServersUnderSameDomain,
    hostname_from_proxied_request_can_be_used_to_refer_to_the_server)
{
    givenMultipleListeningPeersUnderSingleDomain();
    givenHostnameFromProxiedDomainRequest();

    whenSendRequestToUniqueServerHostname();

    thenRequestIsDeliveredToTheSameServer();
}

//-------------------------------------------------------------------------------------------------

class HttpProxyRedirect:
    public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        addRelayInstance();
    }

    void whenSendHttpRequestViaBadRelay()
    {
        sendHttpRequestToPeer(
            listeningPeerHostName(),
            relay(1));
    }
};

TEST_F(HttpProxyRedirect, request_is_redirected_to_a_proxy_relay_instance)
{
    givenListeningPeer();
    whenSendHttpRequestViaBadRelay();
    thenResponseFromPeerIsReceived();
}

//-------------------------------------------------------------------------------------------------

class HttpProxyRedirectToRelayDnsName:
    public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        constexpr int relayCount = 3;

        for (int i = 0; i < relayCount; ++i)
        {
            m_relayNames.push_back("relay_" + std::to_string(i));
            auto relayNameArgument = lm("--server/name=%1")
                .args(m_relayNames.back()).toStdString();
            addRelayInstance({ relayNameArgument.c_str() });
        }

        m_httpClient.setResponseReadTimeout(network::kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(network::kNoTimeout);
    }

    void whenSendHttpRequestViaBadRelay()
    {
        m_httpClient.setMaxNumberOfRedirects(0);
        const auto url = proxyUrlForHost(relay(1), listeningPeerHostName());
        ASSERT_TRUE(m_httpClient.doGet(url));
    }

    void thenRequestHasBeenRedirectedToProperEndpoint()
    {
        ASSERT_EQ(
            network::http::StatusCode::temporaryRedirect,
            m_httpClient.response()->statusLine.statusCode);

        const auto expectedLocationUrl = network::url::Builder()
            .setScheme(network::http::kUrlSchemeName)
            .setEndpoint(network::SocketAddress(
                listeningPeerHostName() + "." + m_relayNames[0],
                network::http::defaultPortForScheme(network::http::kUrlSchemeName)))
            .setPath(m_httpClient.url().path());

        auto actualLocationUrl = nx::utils::Url(
            network::http::getHeaderValue(m_httpClient.response()->headers, "Location"));
        if (actualLocationUrl.port() <= 0)
        {
            actualLocationUrl.setPort(
                network::http::defaultPortForScheme(
                    actualLocationUrl.scheme().toUtf8()));
        }

        ASSERT_EQ(expectedLocationUrl.toString(), actualLocationUrl.toString());
    }

private:
    std::vector<std::string> m_relayNames;
    network::http::HttpClient m_httpClient;
};

TEST_F(
    HttpProxyRedirectToRelayDnsName,
    when_relay_knowns_its_dns_name_redirect_is_done_to_default_port)
{
    givenListeningPeer();
    whenSendHttpRequestViaBadRelay();
    thenRequestHasBeenRedirectedToProperEndpoint();
}

//-------------------------------------------------------------------------------------------------

class HttpProxyNonClusterMode:
    public HttpProxy
{
    using base_type = HttpProxy;

public:
    HttpProxyNonClusterMode():
        base_type(BasicComponentTest::Mode::singleRelay)
    {
    }
};

TEST_F(HttpProxyNonClusterMode, no_cluster_proxy_to_unknown_host_produces_bad_gateway_error)
{
    whenSendHttpRequestToUnknownPeer();

    thenResponseIsReceived();
    andResponseStatusCodeIs(nx::network::http::StatusCode::badGateway);
}

//-------------------------------------------------------------------------------------------------

class HttpProxyWithSsl:
    public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        // Intentionally doing nothing.
    }

    virtual nx::utils::Url proxyUrlForHost(
        const Relay& relayInstance,
        const std::string& hostName) override
    {
        auto url = base_type::proxyUrlForHost(relayInstance, hostName);
        url.setScheme(network::http::kSecureUrlSchemeName);
        url.setPort(relay().moduleInstance()->httpsEndpoints().front().port);
        return url;
    }

    void givenListeningPeerWithSsl()
    {
        givenListeningPeer(network::ssl::EncryptionUse::always);
    }

    void givenRegularRelay()
    {
        addRelay();
    }

    void givenRelayWithLowHandshakeTimeout()
    {
        addRelay(std::chrono::milliseconds(1));
    }

    void whenSendHttpsRequestToPeer()
    {
        auto url = proxyUrlForHost(relay(), listeningPeerHostName());
        url.setScheme(network::http::kSecureUrlSchemeName);
        url.setPort(relay().moduleInstance()->httpsEndpoints().front().port);
        sendHttpRequestToPeer(url);
    }

private:
    void addRelay(
        std::optional<std::chrono::milliseconds> sslHandshakeTimeout = std::nullopt)
    {
        const auto certificateFilePath =
            lm("%1/%2").args(testDataDir(), "traffic_relay.cert").toStdString();

        ASSERT_TRUE(nx::network::ssl::Engine::useOrCreateCertificate(
            certificateFilePath.c_str(),
            "traffic_relay/https test", "US", "Nx"));

        std::vector<const char*> args;
        args.push_back("--https/listenOn=0.0.0.0:0");
        args.push_back("-https/certificatePath");
        args.push_back(certificateFilePath.c_str());
        if (sslHandshakeTimeout)
            args.push_back("--https/sslHandshakeTimeout=1ms");

        addRelayInstance(args);
    }
};

TEST_F(HttpProxyWithSsl, proxying_over_ssl_to_ssl_server_works)
{
    givenRegularRelay();
    givenListeningPeerWithSsl();

    whenSendHttpsRequestToPeer();

    thenResponseFromPeerIsReceived();
}

TEST_F(HttpProxyWithSsl, proxying_over_ssl_to_server_without_ssl_support_produces_502)
{
    givenRelayWithLowHandshakeTimeout();
    givenListeningPeer();

    whenSendHttpsRequestToPeer();

    thenResponseIsReceived();
    andResponseStatusCodeIs(nx::network::http::StatusCode::badGateway);
}

TEST_F(HttpProxyWithSsl, proxy_to_unknown_host_produces_bad_gateway_error)
{
    givenRegularRelay();

    whenSendHttpRequestToUnknownPeer();

    thenResponseIsReceived();
    andResponseStatusCodeIs(nx::network::http::StatusCode::badGateway);
}

//-------------------------------------------------------------------------------------------------

class HttpProxyWithSslStress:
    public HttpProxyWithSsl
{
protected:
    void startSendingConcurrentRequestsToUnknownPeer(int count)
    {
        m_totalRequestCount = count;

        for (int i = 0; i < m_totalRequestCount; ++i)
            whenSendHttpRequestToUnknownPeer();
    }

    void waitForEveryRequestToCompleteWith(
        nx::network::http::StatusCode::Value statusCode)
    {
        for (int i = 0; i < m_totalRequestCount; ++i)
        {
            thenResponseIsReceived();
            andResponseStatusCodeIs(statusCode);
        }
    }

private:
    int m_totalRequestCount = 0;
};

TEST_F(HttpProxyWithSslStress, stress_test)
{
    const auto kRequestCount = std::thread::hardware_concurrency() * 3;

    givenRegularRelay();
    startSendingConcurrentRequestsToUnknownPeer(kRequestCount);
    waitForEveryRequestToCompleteWith(nx::network::http::StatusCode::badGateway);
}

//-------------------------------------------------------------------------------------------------

static const char* kPlaylistPath = "/hls/playlist.m3u8";

static const char* kPlaylist = R"m3u(
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-STREAM-INF:BANDWIDTH=878612,RESOLUTION=640x360,CODECS="avc1.4D4029,mp4a.40.2"
/hls/chunk.ts
#EXT-X-STREAM-INF:BANDWIDTH=2628628,RESOLUTION=1280x720,CODECS="avc1.4D4029,mp4a.40.2"
/hls/chunk.ts
#EXT-X-STREAM-INF:BANDWIDTH=1128660,RESOLUTION=854x480,CODECS="avc1.4D4029,mp4a.40.2"
/hls/chunk.ts
)m3u";

static const char* kBasicChunkPath = "/hls/chunk.ts";

static const char* kChunkData = "chunkchunk";

class HttpProxyHls:
    public HttpProxy
{
    using base_type = HttpProxy;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();
    }

    void givenListeningHlsServer()
    {
        givenListeningPeer();

        listeningPeer().registerStaticProcessor(
            kPlaylistPath,
            kPlaylist,
            "application/vnd.apple.mpegurl",
            network::http::Method::get);

        listeningPeer().registerStaticProcessor(
            kBasicChunkPath,
            kChunkData,
            "text/plain",
            network::http::Method::get);
    }

    void whenReceivedPlaylist()
    {
        auto playlistUrl = proxyUrlForHost(relay(), listeningPeerHostName());
        playlistUrl.setPath(kPlaylistPath);
        sendHttpRequestToPeer(
            playlistUrl,
            network::http::Method::get);

        thenResponseIsReceived();
        andResponseStatusCodeIs(nx::network::http::StatusCode::ok);
    }

    void thenPlaylistContainsAbsoluteUrls()
    {
        extractUrlsFromLastPlaylistResponse();

        for (const auto& url: m_lastPlaylistResponseUrls)
        {
            ASSERT_TRUE(!url.host().isEmpty());
            ASSERT_TRUE(url.path().startsWith("/"));
        }
    }

    void andEachChunkUrlPointExplicitelyToTheContentServer()
    {
        for (const auto& url: m_lastPlaylistResponseUrls)
        {
            addLocalHostAlias(url.host().toStdString());

            sendHttpRequestToPeer(
                url,
                network::http::Method::get);

            thenResponseIsReceived();
            andResponseStatusCodeIs(nx::network::http::StatusCode::ok);
            addMessageBodyIs(kChunkData);
        }
    }

private:
    std::vector<nx::utils::Url> m_lastPlaylistResponseUrls;

    void extractUrlsFromLastPlaylistResponse()
    {
        network::m3u::Playlist playlist;
        ASSERT_TRUE(playlist.parse(lastResponse().messageBody));

        m_lastPlaylistResponseUrls.clear();
        for (const auto& entry: playlist.entries)
        {
            if (entry.type == network::m3u::EntryType::location)
                m_lastPlaylistResponseUrls.push_back(entry.value);
        }
    }
};

TEST_F(HttpProxyHls, urls_in_m3u_playlist_are_rewritten_to_point_to_the_specific_server)
{
    givenListeningHlsServer();

    whenReceivedPlaylist();

    thenPlaylistContainsAbsoluteUrls();
    andEachChunkUrlPointExplicitelyToTheContentServer();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
