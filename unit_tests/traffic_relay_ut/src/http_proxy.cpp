#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
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
    void givenListeningPeer()
    {
        using namespace std::placeholders;

        auto url = basicUrl();
        m_listeningPeerHostName = QnUuid::createUuid().toSimpleString();
        url.setUserName(m_listeningPeerHostName);

        auto acceptor = std::make_unique<RelayConnectionAcceptor>(url);
        m_peerServer = std::make_unique<nx::network::http::TestHttpServer>(
            std::move(acceptor));
        m_peerServer->registerStaticProcessor(
            kTestPath,
            kTestMessage,
            kTestMessageMimeType,
            nx::network::http::Method::get);
        m_peerServer->server().start();

        while (!moduleInstance()->listeningPeerPool().isPeerOnline(
            m_listeningPeerHostName.toStdString()))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void whenSendHttpRequestToPeer()
    {
        m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
        m_httpClient->doGet(
            nx::network::url::Builder(proxyUrlForHost(m_listeningPeerHostName))
                .setPath(kTestPath),
            std::bind(&HttpProxy::saveResponse, this));
    }

    void thenResponseFromPeerIsReceived()
    {
        auto response = m_httpResponseQueue.pop();
        ASSERT_NE(nullptr, response);
        ASSERT_EQ(nx::network::http::StatusCode::ok, response->statusLine.statusCode);
        ASSERT_EQ(kTestMessage, response->messageBody);
    }

    nx::utils::Url proxyUrlForHost(const QString& hostName)
    {
        auto url = basicUrl();

        auto host = lm("%1.gw.%2").args(hostName, url.host()).toQString();
        nx::network::SocketGlobals::addressResolver().addFixedAddress(
            host, network::SocketAddress(network::HostAddress::localhost, url.port()));
        m_registeredHostNames.push_back(host);
        url.setHost(host);

        return url;
    }

private:
    QString m_listeningPeerHostName;
    std::unique_ptr<nx::network::http::TestHttpServer> m_peerServer;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>>
        m_httpResponseQueue;
    std::vector<QString> m_registeredHostNames;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

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
};

TEST_F(HttpProxy, request_is_passed_to_listening_peer)
{
    givenListeningPeer();
    whenSendHttpRequestToPeer();
    thenResponseFromPeerIsReceived();
}

// TEST_F(HttpProxy, works_for_peer_with_complex_name)

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
