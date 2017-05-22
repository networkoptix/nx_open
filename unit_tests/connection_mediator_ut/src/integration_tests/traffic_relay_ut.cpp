#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/utils/std/future.h>

#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class TrafficRelay:
    public MediatorFunctionalTest,
    public api::AbstractCloudSystemCredentialsProvider
{
public:
    TrafficRelay():
        m_relayUrl("http://nxvms.com/relay")
    {
    }

    ~TrafficRelay()
    {
        if (m_serverConnection)
        {
            m_serverConnection->pleaseStopSync();
            m_serverConnection.reset();
        }

        if (m_stunClient)
        {
            m_stunClient->pleaseStopSync();
            m_stunClient.reset();
        }

        if (m_mediatorUdpClient)
        {
            m_mediatorUdpClient->pleaseStopSync();
            m_mediatorUdpClient.reset();
        }

        if (m_mediaServerEmulator)
        {
            m_mediaServerEmulator->pleaseStopSync();
            m_mediaServerEmulator.reset();
        }
    }

protected:
    void issueListenRequest()
    {
        api::ListenRequest request;
        request.systemId = m_systemCredentials.systemId;
        request.serverId = m_systemCredentials.serverId;

        nx::utils::promise<void> done;
        m_serverConnection->listen(
            request,
            [this, &done](
                api::ResultCode resultCode,
                api::ListenResponse response)
            {
                m_lastRequestResult = resultCode;
                m_listenResponse = std::move(response);
                done.set_value();
            });
        done.get_future().wait();
    }

    void givenListeningServer()
    {
        m_mediaServerEmulator->start();
        ASSERT_EQ(api::ResultCode::ok, m_mediaServerEmulator->listen().first);
    }

    void issueConnectRequest()
    {
        api::ConnectRequest request;
        request.connectionMethods = api::ConnectionMethod::all;
        request.connectSessionId = QnUuid::createUuid().toSimpleByteArray();
        request.destinationHostName = m_systemCredentials.systemId;
        request.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();

        nx::utils::promise<void> done;
        m_mediatorUdpClient->connect(
            request,
            [this, &done](
                stun::TransportHeader /*stunTransportHeader*/,
                nx::hpm::api::ResultCode resultCode,
                nx::hpm::api::ConnectResponse response)
            {
                m_lastRequestResult = resultCode;
                m_connectResponse = response;
                done.set_value();
            });
        done.get_future().wait();
    }

    void assertTrafficRelayUrlHasBeenReported()
    {
        ASSERT_EQ(api::ResultCode::ok, m_lastRequestResult);

        if (m_listenResponse)
        {
            ASSERT_TRUE(m_listenResponse->trafficRelayUrl);
            ASSERT_EQ(m_relayUrl, m_listenResponse->trafficRelayUrl->toStdString());
        }

        if (m_connectResponse)
        {
            ASSERT_TRUE(m_connectResponse->trafficRelayUrl);
            ASSERT_EQ(m_relayUrl, m_connectResponse->trafficRelayUrl->toStdString());
        }
    }

private:
    const std::string m_relayUrl;
    api::ResultCode m_lastRequestResult = api::ResultCode::ok;
    boost::optional<api::ListenResponse> m_listenResponse;
    boost::optional<api::ConnectResponse> m_connectResponse;
    std::shared_ptr<nx::stun::AbstractAsyncClient> m_stunClient;
    std::unique_ptr<api::MediatorServerTcpConnection> m_serverConnection;
    std::unique_ptr<api::MediatorClientUdpConnection> m_mediatorUdpClient;
    api::SystemCredentials m_systemCredentials;
    std::unique_ptr<MediaServerEmulator> m_mediaServerEmulator;

    virtual void SetUp() override
    {
        addArg(("--trafficRelay/url=" + m_relayUrl).c_str());

        ASSERT_TRUE(startAndWaitUntilStarted());
        const auto system = addRandomSystem();

        m_systemCredentials.systemId = system.id;
        m_systemCredentials.key = system.authKey;
        m_systemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();

        m_stunClient = std::make_shared<nx::stun::AsyncClient>();
        m_stunClient->connect(stunEndpoint());
        m_serverConnection = 
            std::make_unique<api::MediatorServerTcpConnection>(m_stunClient, this);

        m_mediaServerEmulator = std::make_unique<MediaServerEmulator>(
            stunEndpoint(),
            system);

        m_mediatorUdpClient = std::make_unique<api::MediatorClientUdpConnection>(
            stunEndpoint());
    }

    virtual boost::optional<api::SystemCredentials> getSystemCredentials() const override
    {
        return m_systemCredentials;
    }
};

TEST_F(TrafficRelay, listen_reports_taffic_relay_url)
{
    issueListenRequest();
    assertTrafficRelayUrlHasBeenReported();
}

TEST_F(TrafficRelay, connect_reports_taffic_relay_url)
{
    givenListeningServer();
    issueConnectRequest();
    assertTrafficRelayUrlHasBeenReported();
}

} // namespace test
} // namespace hpm
} // namespase nx
