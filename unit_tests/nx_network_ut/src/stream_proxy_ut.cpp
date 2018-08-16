#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::test {

static constexpr char kTestHandlerPath[] = "/StreamProxy/test";

class StreamProxy:
    public ::testing::Test
{
public:
    ~StreamProxy()
    {
        if (m_httpClient)
            m_httpClient->pleaseStopSync();
    }

protected:
    void givenListeningPingPongServer()
    {
        DestinationContext destinationContext;
        destinationContext.server = std::make_unique<http::TestHttpServer>();
        destinationContext.server->registerStaticProcessor(
            kTestHandlerPath,
            lm("%1").arg(m_destinationServers.size()).toUtf8(),
            "text/plain");
        ASSERT_TRUE(destinationContext.server->bindAndListen());
        m_destinationServers.push_back(std::move(destinationContext));

        auto proxyServer = std::make_unique<network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(proxyServer->setNonBlockingMode(true));
        ASSERT_TRUE(proxyServer->bind(network::SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(proxyServer->listen());
        m_destinationServers.back().proxyEndpoint = proxyServer->getLocalAddress();

        m_destinationServers.back().proxyId = m_proxy.addProxy(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(std::move(proxyServer)),
            m_destinationServers.back().server->serverAddress());
    }

    void givenWorkingProxy()
    {
        givenListeningPingPongServer();
    }

    void whenSendPingViaProxy()
    {
        m_expectedDestinationIndex = 0;
        auto url = url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_destinationServers[m_expectedDestinationIndex].proxyEndpoint)
            .setPath(kTestHandlerPath);

        m_httpClient = std::make_unique<http::AsyncClient>();
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->doGet(url, std::bind(&StreamProxy::saveRequestResult, this));
    }

    void whenReassignToAnotherServer()
    {
        ASSERT_EQ(1U, m_destinationServers.size());
        givenListeningPingPongServer();

        m_proxy.setProxyDestination(
            m_destinationServers[0].proxyId,
            m_destinationServers[1].server->serverAddress());
    }

    void whenStopProxy()
    {
        m_proxy.stopProxy(m_destinationServers.front().proxyId);
    }

    void thenPongIsReceived()
    {
        m_prevResponse = m_requestResults.pop();

        ASSERT_EQ(SystemError::noError, m_prevResponse->osResultCode);
        ASSERT_NE(nullptr, m_prevResponse->response);
        ASSERT_EQ(http::StatusCode::ok, m_prevResponse->response->statusLine.statusCode);
        ASSERT_EQ(
            lm("%1").arg(m_expectedDestinationIndex).toUtf8(),
            m_prevResponse->response->messageBody);
    }

    void thenTrafficProxiedToAnotherServer()
    {
        whenSendPingViaProxy();

        m_expectedDestinationIndex = 1;

        thenPongIsReceived();
    }

    void thenProxyEndpointIsNotAvailable()
    {
        whenSendPingViaProxy();

        thenRequestHasFailed();
        andEndpointIsNotAvailable();
    }

    void thenRequestHasFailed()
    {
        m_prevResponse = m_requestResults.pop();

        ASSERT_EQ(nullptr, m_prevResponse->response);
    }

    void andEndpointIsNotAvailable()
    {
        ASSERT_NE(SystemError::noError, m_prevResponse->osResultCode);
    }

private:
    struct DestinationContext
    {
        std::unique_ptr<http::TestHttpServer> server;
        SocketAddress proxyEndpoint;
        int proxyId = -1;
    };

    struct RequestResult
    {
        std::unique_ptr<http::Response> response;
        SystemError::ErrorCode osResultCode = SystemError::noError;
    };

    network::StreamProxy m_proxy;
    std::vector<DestinationContext> m_destinationServers;
    std::unique_ptr<http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<RequestResult> m_requestResults;
    std::optional<RequestResult> m_prevResponse;
    int m_expectedDestinationIndex = -1;

    void saveRequestResult()
    {
        RequestResult requestResult;
        if (m_httpClient->response())
        {
            requestResult.response =
                std::make_unique<http::Response>(*m_httpClient->response());
            requestResult.response->messageBody =
                m_httpClient->fetchMessageBodyBuffer();
        }
        requestResult.osResultCode = m_httpClient->lastSysErrorCode();

        m_httpClient.reset();

        m_requestResults.push(std::move(requestResult));
    }
};

TEST_F(StreamProxy, data_is_delivered_to_the_target_server)
{
    givenListeningPingPongServer();
    whenSendPingViaProxy();
    thenPongIsReceived();
}

TEST_F(StreamProxy, change_proxy_destination)
{
    givenWorkingProxy();
    whenReassignToAnotherServer();
    thenTrafficProxiedToAnotherServer();
}

TEST_F(StreamProxy, stop_proxy)
{
    givenWorkingProxy();
    whenStopProxy();
    thenProxyEndpointIsNotAvailable();
}

} // namespace nx::network::test
