#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

#include "basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

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

        if (m_httpClient)
        {
            m_httpClient->pleaseStopSync();
            m_httpClient.reset();
        }

        for (const auto& hostName: m_registeredHostNames)
            nx::network::SocketGlobals::addressResolver().removeFixedAddress(hostName);
    }

protected:
    virtual void SetUp() override
    {
        addRelayInstance();
    }

    void givenListeningPeer()
    {
        using namespace std::placeholders;

        auto url = relay().basicUrl();
        if (m_listeningPeerHostName.empty())
        {
            m_listeningPeerHostName =
                QnUuid::createUuid().toSimpleByteArray().toStdString();
        }
        url.setUserName(m_listeningPeerHostName.c_str());

        auto acceptor = std::make_unique<RelayConnectionAcceptor>(url);
        m_peerServer = std::make_unique<nx::network::http::TestHttpServer>(
            std::move(acceptor));
        m_peerServer->registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpProxy::processHttpRequest, this, _1, _2, _3, _4, _5),
            nx::network::http::Method::get);
        m_peerServer->server().start();

        while (!peerInformationSynchronizedInCluster(m_listeningPeerHostName))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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
        m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
        m_httpClient->setAdditionalHeaders(std::move(headers));
        m_httpClient->setResponseReadTimeout(std::chrono::hours(1));
        m_httpClient->doRequest(
            method,
            nx::network::url::Builder(proxyUrlForHost(relayInstance, hostName))
                .setPath(kTestPath),
            std::bind(&HttpProxy::saveResponse, this));

        m_lastRequest = m_httpClient->request();
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

        ASSERT_EQ(kTestMessage, m_lastResponse->messageBody);
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

    const std::string& listeningPeerHostName() const
    {
        return m_listeningPeerHostName;
    }

    const nx::network::http::Response& lastResponse() const
    {
        return *m_lastResponse;
    }

private:
    std::string m_listeningPeerHostName;
    std::unique_ptr<nx::network::http::TestHttpServer> m_peerServer;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>>
        m_httpResponseQueue;
    std::vector<std::string> m_registeredHostNames;
    std::unique_ptr<nx::network::http::Response> m_lastResponse;
    network::http::Request m_lastRequest;
    int m_requestsServedByPeerCount = 0;

    void saveResponse()
    {
        if (m_httpClient->response())
        {
            auto response = std::make_unique<nx::network::http::Response>(
                *m_httpClient->response());
            response->messageBody = m_httpClient->fetchMessageBodyBuffer();
            m_httpResponseQueue.push(std::move(response));
        }
        else
        {
            m_httpResponseQueue.push(nullptr);
        }
    }

    nx::utils::Url proxyUrlForHost(
        const Relay& relayInstance,
        const std::string& hostName)
    {
        auto url = relayInstance.basicUrl();

        auto host = lm("%1.%2").args(hostName, url.host()).toStdString();
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            host, network::SocketAddress(network::HostAddress::localhost, 0));
        m_registeredHostNames.push_back(host);
        url.setHost(host.c_str());

        return url;
    }

    void processHttpRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        ++m_requestsServedByPeerCount;

        network::http::RequestResult requestResult(network::http::StatusCode::ok);
        requestResult.dataSource =
            std::make_unique<network::http::BufferSource>(kTestMessageMimeType, kTestMessage);
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

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
