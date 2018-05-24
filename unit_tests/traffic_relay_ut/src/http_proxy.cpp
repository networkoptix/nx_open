#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/http/http_async_client.h>
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

class HttpProxy:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    ~HttpProxy()
    {
        if (m_acceptor)
        {
            m_acceptor->pleaseStopSync();
            m_acceptor.reset();
        }

        if (m_httpClient)
        {
            m_httpClient->pleaseStopSync();
            m_httpClient.reset();
        }

        for (auto& connection: m_acceptedConnections)
            connection->pleaseStopSync();
        m_acceptedConnections.clear();

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

        m_acceptor = std::make_unique<RelayConnectionAcceptor>(url);
        m_acceptor->acceptAsync(std::bind(
            &HttpProxy::saveAcceptedConnection, this, _1, _2));

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
                .setPath("/api/moduleInformation"),
            std::bind(&HttpProxy::saveResponse, this));
    }

    void thenRequestIsReceived()
    {
        ASSERT_NE(nullptr, m_receivedHttpRequests.pop());
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
    std::unique_ptr<RelayConnectionAcceptor> m_acceptor;
    std::vector<std::unique_ptr<nx::network::http::AsyncMessagePipeline>>
        m_acceptedConnections;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>>
        m_httpResponseQueue;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Request>>
        m_receivedHttpRequests;
    std::vector<QString> m_registeredHostNames;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void saveAcceptedConnection(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        using namespace std::placeholders;

        if (systemErrorCode != SystemError::noError)
            return;

        auto httpConnection = std::make_unique<nx::network::http::AsyncMessagePipeline>(
            nullptr, std::move(connection));
        httpConnection->setMessageHandler(
            std::bind(&HttpProxy::saveHttpRequest, this, _1));
        httpConnection->startReadingConnection();
        m_acceptedConnections.push_back(std::move(httpConnection));
    }

    void saveHttpRequest(nx::network::http::Message requestMsg)
    {
        if (requestMsg.type != nx::network::http::MessageType::request)
        {
            m_receivedHttpRequests.push(nullptr);
        }
        else
        {
            m_receivedHttpRequests.push(
                std::make_unique<nx::network::http::Request>(*requestMsg.request));
        }
    }

    void saveResponse()
    {
        if (m_httpClient->response())
        {
            m_httpResponseQueue.push(
                std::make_unique<nx::network::http::Response>(
                    *m_httpClient->response()));
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
    thenRequestIsReceived();
}

// TEST_F(HttpProxy, works_for_peer_with_complex_name)

// TEST_F(HttpProxy, m3u_playlist_url_host_gets_rewritten)

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
