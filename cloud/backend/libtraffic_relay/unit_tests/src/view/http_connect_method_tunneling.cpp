#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_connect.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/sync_queue.h>

#include "../basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class TunnelingUsingHttpConnectMethod:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    TunnelingUsingHttpConnectMethod():
        m_systemId(QnUuid::createUuid().toSimpleString().toStdString()),
        m_serverId(QnUuid::createUuid().toSimpleString().toStdString())
    {
    }

    ~TunnelingUsingHttpConnectMethod()
    {
        m_httpClient.pleaseStopSync();
        if (m_relayClient)
            m_relayClient->pleaseStopSync();
        if (m_serverToClientConnection)
            m_serverToClientConnection->pleaseStopSync();
        if (m_clientToServerConnection)
            m_clientToServerConnection->pleaseStopSync();
    }

protected:
    void whenServerSendsConnectRequestToOpenTunnel()
    {
        m_httpClient.doConnect(
            relay().basicUrl(),
            lm("%1.%2").args(m_serverId, m_systemId).toUtf8(),
            std::bind(&TunnelingUsingHttpConnectMethod::saveEstablishServerTunnelResult, this));
    }

    void whenServerEstablishedTunnel()
    {
        whenServerSendsConnectRequestToOpenTunnel();
        thenOkResponseIsReceived();
    }

    void thenOkResponseIsReceived()
    {
        const auto response = m_responseQueue.pop();

        ASSERT_NE(nullptr, response.get());
        ASSERT_EQ(nx::network::http::StatusCode::ok, response->statusLine.statusCode);
        ASSERT_TRUE(
            nx::network::http::getHeaderValue(response->headers, "Content-Length").isEmpty() ||
            nx::network::http::getHeaderValue(response->headers, "Content-Length") == "0");
        ASSERT_TRUE(response->headers.find("Content-Type") == response->headers.end());
    }

    void thenConnectionToServerCanBeEstablished()
    {
        std::promise<std::tuple<api::ResultCode, api::CreateClientSessionResponse>> done;
        m_relayClient->startSession(
            "", m_systemId.c_str(),
            [&done](api::ResultCode resultCode, api::CreateClientSessionResponse response)
            {
                done.set_value({resultCode, std::move(response)});
            });
        const auto createSessionResult = done.get_future().get();
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(createSessionResult));

        std::promise<std::tuple<
            api::ResultCode, std::unique_ptr<network::AbstractStreamSocket>>
        > connectionCreated;
        m_relayClient->openConnectionToTheTargetHost(
            std::get<1>(createSessionResult).sessionId.c_str(),
            [&connectionCreated](
                api::ResultCode resultCode,
                std::unique_ptr<network::AbstractStreamSocket> connection)
            {
                connectionCreated.set_value({resultCode, std::move(connection)});
            });
        auto openConnectionResult = connectionCreated.get_future().get();
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(openConnectionResult));
        m_clientToServerConnection = std::move(std::get<1>(openConnectionResult));
        ASSERT_NE(nullptr, m_clientToServerConnection);
    }

private:
    const std::string m_systemId;
    const std::string m_serverId;
    nx::network::http::AsyncClient m_httpClient;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>> m_responseQueue;
    std::unique_ptr<api::AbstractClient> m_relayClient;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_serverToClientConnection;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_clientToServerConnection;

    virtual void SetUp() override
    {
        addRelayInstance();

        m_relayClient = std::make_unique<api::detail::ClientOverHttpConnect>(
            relay().basicUrl(), nullptr);
    }

    void saveEstablishServerTunnelResult()
    {
        if (m_httpClient.response())
        {
            m_responseQueue.push(std::make_unique<nx::network::http::Response>(
                *m_httpClient.response()));
        }
        else
        {
            m_responseQueue.push(nullptr);
        }

        m_serverToClientConnection = m_httpClient.takeSocket();
    }
};

TEST_F(
    TunnelingUsingHttpConnectMethod,
    server_can_establish_tunnel_using_connect)
{
    whenServerSendsConnectRequestToOpenTunnel();
    thenOkResponseIsReceived();
}

TEST_F(
    TunnelingUsingHttpConnectMethod,
    client_can_establish_connection_to_server_using_connect)
{
    whenServerEstablishedTunnel();
    thenConnectionToServerCanBeEstablished();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
