#include "mediator_relay_integration_test_setup.h"

#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace hpm {
namespace test {

namespace {

class RelayClusterClientStub:
    public AbstractRelayClusterClient
{
public:
    RelayClusterClientStub(QUrl relayUrl):
        m_relayUrl(relayUrl)
    {
    }

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& /*peerId*/,
        RelayInstanceSearchCompletionHandler completionHandler) override
    {
        m_aioThreadBinder.post(
            [this, completionHandler = std::move(completionHandler)]()
            {
                completionHandler(cloud::relay::api::ResultCode::ok, m_relayUrl);
            });
    }

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& /*peerId*/,
        RelayInstanceSearchCompletionHandler completionHandler) override
    {
        m_aioThreadBinder.post(
            [this, completionHandler = std::move(completionHandler)]()
            {
                completionHandler(cloud::relay::api::ResultCode::ok, m_relayUrl);
            });
    }

private:
    network::aio::BasicPollable m_aioThreadBinder;
    QUrl m_relayUrl;
};

} // namespace

//-------------------------------------------------------------------------------------------------

MediatorRelayIntegrationTestSetup::MediatorRelayIntegrationTestSetup():
    m_relayUrl("http://nxvms.com/relay")
{
    m_factoryFuncBak = RelayClusterClientFactory::instance().setCustomFunc(
        std::bind(&MediatorRelayIntegrationTestSetup::createRelayClusterClient, this, std::placeholders::_1));
}

MediatorRelayIntegrationTestSetup::~MediatorRelayIntegrationTestSetup()
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

    RelayClusterClientFactory::instance().setCustomFunc(std::move(m_factoryFuncBak));
}

void MediatorRelayIntegrationTestSetup::SetUp()
{
    //addArg(("--trafficRelay/url=" + m_relayUrl.toString().toStdString()).c_str());

    ASSERT_TRUE(startAndWaitUntilStarted());
    const auto system = addRandomSystem();

    m_systemCredentials.systemId = system.id;
    m_systemCredentials.key = system.authKey;
    m_systemCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();

    m_stunClient = std::make_shared<nx::stun::AsyncClient>();
    m_stunClient->connect(
        nx::network::url::Builder().setScheme(nx::stun::kUrlSchemeName)
        .setEndpoint(stunEndpoint()));
    m_serverConnection =
        std::make_unique<api::MediatorServerTcpConnection>(m_stunClient, this);

    m_mediaServerEmulator = std::make_unique<MediaServerEmulator>(
        stunEndpoint(),
        system);

    m_mediatorUdpClient = std::make_unique<api::MediatorClientUdpConnection>(
        stunEndpoint());
}

void MediatorRelayIntegrationTestSetup::issueListenRequest()
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

void MediatorRelayIntegrationTestSetup::givenListeningServer()
{
    m_mediaServerEmulator->start();
    ASSERT_EQ(api::ResultCode::ok, m_mediaServerEmulator->listen().first);
}

void MediatorRelayIntegrationTestSetup::issueConnectRequest()
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

void MediatorRelayIntegrationTestSetup::assertTrafficRelayUrlHasBeenReported()
{
    ASSERT_EQ(api::ResultCode::ok, m_lastRequestResult);

    if (m_listenResponse)
    {
        ASSERT_TRUE(!m_listenResponse->trafficRelayUrls.empty());
        ASSERT_EQ(
            m_relayUrl.toString().toStdString(),
            m_listenResponse->trafficRelayUrls.front().toStdString());
    }

    if (m_connectResponse)
    {
        ASSERT_TRUE(m_connectResponse->trafficRelayUrl);
        ASSERT_EQ(
            m_relayUrl.toString().toStdString(),
            m_connectResponse->trafficRelayUrl->toStdString());
    }
}

boost::optional<nx::String> MediatorRelayIntegrationTestSetup::reportedTrafficRelayUrl()
{
    if (m_listenResponse)
        return m_listenResponse->trafficRelayUrl;

    if (m_connectResponse)
        return m_connectResponse->trafficRelayUrl;

    return boost::none;
}

boost::optional<api::SystemCredentials> MediatorRelayIntegrationTestSetup::getSystemCredentials() const
{
    return m_systemCredentials;
}

std::unique_ptr<AbstractRelayClusterClient> MediatorRelayIntegrationTestSetup::createRelayClusterClient(
    const conf::Settings& /*settings*/)
{
    return std::make_unique<RelayClusterClientStub>(m_relayUrl);
}

} // namespace test
} // namespace hpm
} // namespace nx
