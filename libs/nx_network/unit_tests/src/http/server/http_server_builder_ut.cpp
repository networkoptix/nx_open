// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/metrics_request_handler.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/prometheus/registry.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::server::test {

namespace {

class TestRequestHandler: public AbstractRequestHandler
{
public:
    virtual void serve(
        network::http::RequestContext /*requestContext*/,
        nx::MoveOnlyFunc<void(network::http::RequestResult)> handler) override
    {
        handler(StatusCode::noContent);
    }
};

// Never invokes the completion handler, simulating an application handler that keeps the
// request (and the metrics RequestRecorder moved into its completion handler) in flight beyond
// this call.
class StoringRequestHandler: public AbstractRequestHandler
{
public:
    std::optional<nx::MoveOnlyFunc<void(network::http::RequestResult)>> storedCompletionHandler;

    virtual void serve(
        network::http::RequestContext /*requestContext*/,
        nx::MoveOnlyFunc<void(network::http::RequestResult)> handler) override
    {
        storedCompletionHandler.emplace(std::move(handler));
    }
};

} // namespace

class HttpServerBuilder:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    HttpServerBuilder()
    {
        m_settings.endpoints.push_back("127.0.0.1:0");
        m_settings.ssl.endpoints.push_back("127.0.0.1:0");

        m_client.setTimeouts(http::AsyncClient::kInfiniteTimeouts);
    }

    void buildServer()
    {
        SystemError::ErrorCode err = SystemError::noError;
        std::tie(m_server, err) = Builder::build(settings(), &m_requestHandler);
        ASSERT_EQ(SystemError::noError, err) << SystemError::toString(err);
        ASSERT_NE(nullptr, m_server);
    }

    MultiEndpointServer& server() { return *m_server; }
    std::unique_ptr<MultiEndpointServer> takeServer() { return std::exchange(m_server, nullptr); }

protected:
    Settings& settings() { return m_settings; }
    AbstractRequestHandler* requestHandler() { return &m_requestHandler; }

    void enableSslServerCertificate()
    {
        m_issuer = ssl::X509Name{"nx_network/HttpServerBuilder test", "US", "Nx"};

        settings().ssl.certificatePath = testDataDir().toStdString() + "/test.cert";
        const auto certData = makeCertificateAndKey(m_issuer, "localhost");
        std::ofstream f(
            settings().ssl.certificatePath,
            std::ios_base::binary | std::ios_base::trunc);
        ASSERT_TRUE(f.is_open());
        f << certData;
    }

    int allocateReusablePort()
    {
        settings().reusePort = true;
        buildServer();
        m_bakServers.push_back(takeServer());
        return m_bakServers.front()->endpoints().front().port;
    }

    void whenEnableRedirectHttpToHttps() { settings().redirectHttpToHttps = true; }

    void whenMakeRequestOnHttp()
    {
        nx::Url httpUrl = *std::find_if(server().urls().begin(), server().urls().end(),
            [](const auto& url)
            {
                return url.scheme() == nx::network::http::kUrlSchemeName;
            });
        ASSERT_TRUE(m_client.doGet(httpUrl));
    }

    void thenContentLocationUrlIsHttps()
    {
        ASSERT_EQ(m_client.contentLocationUrl().scheme(), nx::network::http::kSecureUrlSchemeName);
    }

    void assertServerAcceptsHttpRequests()
    {
        whenMakeRequestOnHttp();

        ASSERT_NE(nullptr, m_client.response());
    }

    void assertThereAreNListenersOnGivenPort(int expectedListenerCnt, int port)
    {
        int cnt = 0;
        server().forEachListener(
            [port, &cnt](nx::network::http::HttpStreamSocketServer* server)
            {
                if (server->address().port == port)
                    ++cnt;
            });

        ASSERT_EQ(expectedListenerCnt, cnt);
    }

    void assertListenersAreDistributedAcrossAioThreadPool()
    {
        std::set<aio::AbstractAioThread*> usedAioThreads;
        std::size_t listenerCnt = 0;
        server().forEachListener(
            [&usedAioThreads, &listenerCnt](nx::network::http::HttpStreamSocketServer* server)
            {
                usedAioThreads.insert(server->getAioThread());
                ++listenerCnt;
            });

        const std::size_t aioThreadCnt =
            SocketGlobals::instance().aioService().getAllAioThreads().size();

        ASSERT_EQ(usedAioThreads.size(), std::min(listenerCnt, aioThreadCnt));
    }

    void assertServerUsesTheProvidedCertificate()
    {
        ssl::ClientStreamSocket socket(
            ssl::Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            ssl::kAcceptAnyCertificateCallback);

        socket.setSyncVerifyCertificateChainCallback(
            [this](auto&&... args)
            {
                return verifyServerCertificate(std::forward<decltype(args)>(args)...);
            });

        nx::Url httpsUrl = *std::find_if(server().urls().begin(), server().urls().end(),
            [](const auto& url)
            {
                return url.scheme() == nx::network::http::kSecureUrlSchemeName;
            });

        ASSERT_TRUE(socket.connect(url::getEndpoint(httpsUrl), kNoTimeout))
            << SystemError::getLastOSErrorText();

        ASSERT_EQ(m_issuer, m_lastVerifiedCertificate.issuer());
    }

    const Response* clientResponse() { return m_client.response(); }

private:
    bool verifyServerCertificate(
        nx::network::ssl::CertificateChainView chain,
        nx::network::ssl::StreamSocket* /*socket*/)
    {
        m_lastVerifiedCertificate = nx::network::ssl::Certificate(chain.front());
        return true;
    }

private:
    Settings m_settings;
    TestRequestHandler m_requestHandler;
    std::unique_ptr<MultiEndpointServer> m_server;
    HttpClient m_client{ssl::kAcceptAnyCertificate};
    ssl::X509Name m_issuer;
    ssl::Certificate m_lastVerifiedCertificate;
    std::vector<std::unique_ptr<MultiEndpointServer>> m_bakServers;
};

TEST_F(HttpServerBuilder, preferred_url_has_listening_endpoint)
{
    buildServer();

    ASSERT_EQ(
        nx::Url(nx::format("https://127.0.0.1:%1").args(server().endpoints().back().port)),
        server().preferredUrl());
}

TEST_F(HttpServerBuilder, preferred_url_has_given_server_name)
{
    settings().serverName = "example.com";

    buildServer();

    ASSERT_EQ(
        nx::Url(nx::format("https://example.com:%1").args(server().endpoints().back().port)),
        server().preferredUrl());
}

TEST_F(HttpServerBuilder, preferred_url_has_given_server_name_and_port)
{
    settings().serverName = "example.com:443";

    buildServer();

    ASSERT_EQ(nx::Url("https://example.com:443"), server().preferredUrl());
}

TEST_F(HttpServerBuilder, redirect_http_to_https)
{
    whenEnableRedirectHttpToHttps();

    buildServer();
    ASSERT_TRUE(server().listen());

    whenMakeRequestOnHttp();

    thenContentLocationUrlIsHttps();
}

TEST_F(HttpServerBuilder, ssl_certificate_is_loaded)
{
    enableSslServerCertificate();

    buildServer();
    ASSERT_TRUE(server().listen());

    assertServerUsesTheProvidedCertificate();
}

// Testing that it allows distribution of connection accept across multiple listeners.
TEST_F(HttpServerBuilder, multiple_listeners_on_the_same_local_port)
{
    settings().ssl.endpoints.clear();

    settings().endpoints.front().port = allocateReusablePort();
    settings().reusePort = false;
    // reusePort is expected to be applied automatically when concurrency is enabled.
    settings().listeningConcurrency = 5;

    buildServer();
    ASSERT_TRUE(server().listen());

    assertServerAcceptsHttpRequests();
    // Checking that there are really multiple sockets listening for incoming connections.
    assertThereAreNListenersOnGivenPort(
        settings().listeningConcurrency, settings().endpoints.front().port);
    assertListenersAreDistributedAcrossAioThreadPool();
}

TEST_F(HttpServerBuilder, extra_success_response_headers)
{
    settings().extraSuccessResponseHeaders.emplace(
        header::Server::NAME,
        compatibilityServerName());
    settings().extraSuccessResponseHeaders.emplace("Hello", "World");

    buildServer();
    ASSERT_TRUE(server().listen());

    assertServerAcceptsHttpRequests();

    ASSERT_EQ(compatibilityServerName(), clientResponse()->headers.find(header::Server::NAME)->second);
    ASSERT_EQ("World", clientResponse()->headers.find("Hello")->second);
}

TEST_F(HttpServerBuilder, metrics_registry_wiring_records_golden_signals)
{
    nx::prometheus::Registry registry("testService", "dev");

    SystemError::ErrorCode err = SystemError::noError;
    std::unique_ptr<MultiEndpointServer> server;
    std::tie(server, err) =
        Builder::build(settings(), requestHandler(), &registry, /*serverName*/ "public");
    ASSERT_EQ(SystemError::noError, err) << SystemError::toString(err);
    ASSERT_NE(nullptr, server);
    ASSERT_TRUE(server->listen());

    HttpClient client{ssl::kAcceptAnyCertificate};
    const auto httpUrl = *std::find_if(
        server->urls().begin(),
        server->urls().end(),
        [](const auto& url)
        {
            return url.scheme() == nx::network::http::kUrlSchemeName;
        });
    ASSERT_TRUE(client.doGet(httpUrl));

    // TestRequestHandler responds 204 (noContent) with no route template.
    const std::string serialized = registry.serialize();
    EXPECT_NE(
        serialized.find("# TYPE http_server_request_duration_seconds histogram"),
        std::string::npos);
    EXPECT_NE(serialized.find("http_response_status_code=\"204\""), std::string::npos);
    EXPECT_NE(serialized.find("# TYPE http_server_active_requests gauge"), std::string::npos);
    EXPECT_NE(
        serialized.find(HttpRequestMetrics::kLabelServerName + "=\"public\""),
        std::string::npos);
}

TEST_F(HttpServerBuilder, requests_in_flight_at_server_destruction_do_not_crash)
{
    nx::prometheus::Registry registry("testService", "dev");
    StoringRequestHandler storingHandler;

    SystemError::ErrorCode err = SystemError::noError;
    std::unique_ptr<MultiEndpointServer> server;
    std::tie(server, err) =
        Builder::build(settings(), &storingHandler, &registry, /*serverName*/ "public");
    ASSERT_EQ(SystemError::noError, err) << SystemError::toString(err);
    ASSERT_NE(nullptr, server);
    ASSERT_TRUE(server->listen());

    const auto httpUrl = *std::find_if(
        server->urls().begin(),
        server->urls().end(),
        [](const auto& url)
        {
            return url.scheme() == nx::network::http::kUrlSchemeName;
        });

    // A raw socket (rather than HttpClient) so sending the request never blocks waiting for a
    // response that this handler will never produce.
    Request request;
    request.requestLine.method = Method::get;
    request.requestLine.url = "/ping";
    request.requestLine.version = http_1_1;
    const auto serializedRequest = request.serialized();

    TCPSocket connection(AF_INET);
    ASSERT_TRUE(connection.connect(url::getEndpoint(httpUrl), kNoTimeout));
    ASSERT_EQ(
        serializedRequest.size(),
        connection.send(serializedRequest.data(), serializedRequest.size()));

    while (!storingHandler.storedCompletionHandler)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Exercises ~MultiEndpointServer: it must stop all listeners (via pleaseStopSync) before
    // the metrics objects it owns are destroyed, so no AIO thread can touch them afterwards.
    server.reset();

    (*storingHandler.storedCompletionHandler)(StatusCode::ok);
    storingHandler.storedCompletionHandler.reset();

    EXPECT_DOUBLE_EQ(
        registry
            .gauge(
                "http_server_active_requests",
                "Number of in-flight HTTP server requests",
                {{HttpRequestMetrics::kLabelServerName, "public"}})
            .value(),
        0.0);
}

} // namespace nx::network::http::server::test
