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

    void whenSendPingViaProxy()
    {
        auto url = url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_destinationServers.front().proxyEndpoint);

        m_httpClient = std::make_unique<http::AsyncClient>();
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->doGet(url, std::bind(&StreamProxy::saveRequestResult, this));
    }

    void thenPongIsReceived()
    {
        m_prevResponse = m_requestResults.pop();
        ASSERT_EQ(
            http::StatusCode::notFound,
            (*m_prevResponse)->statusLine.statusCode);
    }

private:
    struct DestinationContext
    {
        std::unique_ptr<http::TestHttpServer> server;
        SocketAddress proxyEndpoint;
        int proxyId = -1;
    };

    network::StreamProxy m_proxy;
    std::vector<DestinationContext> m_destinationServers;
    std::unique_ptr<http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<std::unique_ptr<http::Response>> m_requestResults;
    std::optional<std::unique_ptr<http::Response>> m_prevResponse;

    void saveRequestResult()
    {
        std::unique_ptr<http::Response> response =
            m_httpClient->response()
            ? std::make_unique<http::Response>(*m_httpClient->response())
            : nullptr;

        m_httpClient.reset();

        if (response)
            m_requestResults.push(std::move(response));
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
    // TODO
}

} // namespace nx::network::test
