#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>
#include <nx/utils/sync_call.h>

#include <discovery/registered_peer_pool.h>

namespace nx {
namespace cloud {
namespace discovery {
namespace test {

static const char* const kPeerType = "test_peer";
static const nx::Buffer kWebsocketProtocolName = "websocket";
static const char* const kTestPath = "/create_websocket";

struct PeerInformation:
    BasicInstanceInformation
{
    PeerInformation():
        BasicInstanceInformation(kPeerType)
    {
    }
};

class DiscoveryRegisteredPeerPool:
    public ::testing::Test
{
public:
    DiscoveryRegisteredPeerPool():
        m_registeredPeerPool(m_configuration),
        m_peerId(QnUuid::createUuid().toStdString())
    {
    }

    ~DiscoveryRegisteredPeerPool()
    {
        if (m_peerConnection)
            m_peerConnection->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessor<nx_http::server::handler::CreateTunnelHandler>(
            kTestPath,
            [this]()
            {
                return std::make_unique<nx_http::server::handler::CreateTunnelHandler>(
                    kWebsocketProtocolName,
                    std::bind(&DiscoveryRegisteredPeerPool::onUpgradedConnectionAccepted, this, _1));
            });

        //nx::network::websocket::addClientHeaders(&request, kP2pProtoName);

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void givenConnectedPeer()
    {
        nx_http::HttpClient httpClient;
        ASSERT_TRUE(httpClient.doUpgrade(getUrl(), kWebsocketProtocolName));
        ASSERT_TRUE(nx_http::StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
            << httpClient.response()->statusLine.statusCode;

        auto streamSocket = httpClient.takeSocket();
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));
        auto peerConnection = std::make_unique<nx::network::WebSocket>(std::move(streamSocket));
        m_registeredPeerPool.addPeerConnection(
            m_peerId,
            std::move(peerConnection));
    }

    void givenReportedPeer()
    {
        givenConnectedPeer();
        whenPeerSendsInformation();
    }

    void givenMultipleOnlinePeersOfDifferentTypes()
    {
        // TODO
    }

    void whenRequestPeerWithRandomId()
    {
        whenRequestPeerInfo();
    }

    void whenRequestPeerInfo()
    {
        m_foundPeer = m_registeredPeerPool.getPeerInfo(m_peerId);
    }

    void whenPeerConnectionIsBroken()
    {
        m_peerConnection->pleaseStopSync();
        m_peerConnection.reset();
    }

    void whenPeerSendsInformation()
    {
        PeerInformation peerInfo;
        peerInfo.id = m_peerId;
        peerInfo.apiUrl = "http://nxvms.com/test_peer/";
        peerInfo.status = PeerStatus::online;
        m_expectedPeerInfo = QJson::serialized(peerInfo);
        sendPeerInfo(m_expectedPeerInfo);
    }

    void whenPeerSendsInvalidInformation()
    {
        sendPeerInfo("invalid json");
    }

    void whenRequestPeersOfSpecificType()
    {
        // TODO
    }

    void whenRequestPeersOfUnknownType()
    {
        // TODO
    }

    void thenPeerWasNotFound()
    {
        ASSERT_FALSE(static_cast<bool>(m_foundPeer));
    }

    void thenPeerIsFound()
    {
        ASSERT_TRUE(static_cast<bool>(m_foundPeer));
        // TODO Verifying peer information.
    }

    void thenPeerMovedToOnlineState()
    {
        while (!m_registeredPeerPool.getPeerInfo(m_peerId))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeerMovedToOfflineState()
    {
        while (m_registeredPeerPool.getPeerInfo(m_peerId))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeerInformationIsUpdated()
    {
        for (;;)
        {
            auto info = m_registeredPeerPool.getPeerInfo(m_peerId);
            if (info && *info == m_expectedPeerInfo.toStdString())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thenConnectionIsBroken()
    {
        nx::Buffer readBuf;

        for (;;)
        {
            SystemError::ErrorCode systemErrorCode = SystemError::noError;
            std::size_t bytesRead = 0;
            std::tie(systemErrorCode, bytesRead) =
                makeSyncCall<SystemError::ErrorCode, std::size_t>(std::bind(
                    &nx::network::WebSocket::readSomeAsync,
                    m_peerConnection.get(),
                    &readBuf,
                    std::placeholders::_1));

            if (systemErrorCode == SystemError::noError && bytesRead == 0 ||
                nx::network::socketCannotRecoverFromError(systemErrorCode))
            {
                break;
            }
        }
    }

    void thenOnlyPeersOfThatTypeAreReported()
    {
        // TODO
    }

    void thenEmptyListIsReturned()
    {
        // TODO
    }

private:
    conf::Discovery m_configuration;
    discovery::RegisteredPeerPool m_registeredPeerPool;
    std::string m_peerId;
    boost::optional<std::string> m_foundPeer;
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::WebSocket>> m_acceptedConnections;
    std::unique_ptr<nx::network::WebSocket> m_peerConnection;
    nx::Buffer m_expectedPeerInfo;

    void onUpgradedConnectionAccepted(std::unique_ptr<AbstractStreamSocket> connection)
    {
        auto webSocket = std::make_unique<nx::network::WebSocket>(
            std::move(connection));
        m_acceptedConnections.push(std::move(webSocket));
    }

    QUrl getUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kTestPath);
    }

    void sendPeerInfo(const nx::Buffer& peerInfoJson)
    {
        m_peerConnection = m_acceptedConnections.pop();

        nx::utils::promise<SystemError::ErrorCode> sent;
        m_peerConnection->sendAsync(
            peerInfoJson,
            [this, &sent](
                SystemError::ErrorCode systemErrorCode,
                std::size_t /*bytesWritten*/)
            {
                sent.set_value(systemErrorCode);
            });
        ASSERT_EQ(SystemError::noError, sent.get_future().get());
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(DiscoveryRegisteredPeerPool, peer_which_has_reported_its_info_is_online)
{
    givenReportedPeer();
    thenPeerMovedToOnlineState();
}

TEST_F(DiscoveryRegisteredPeerPool, unknown_peer_is_reported_as_offline)
{
    whenRequestPeerWithRandomId();
    thenPeerWasNotFound();
}

TEST_F(DiscoveryRegisteredPeerPool, peer_becomes_offline_after_dropping_connection)
{
    givenReportedPeer();
    whenPeerConnectionIsBroken();
    thenPeerMovedToOfflineState();
}

TEST_F(DiscoveryRegisteredPeerPool, peer_information_is_received_via_websocket)
{
    givenConnectedPeer();
    whenPeerSendsInformation();
    thenPeerInformationIsUpdated();
}

TEST_F(DiscoveryRegisteredPeerPool, peer_which_has_not_yet_reported_info_is_offline)
{
    givenConnectedPeer();
    whenRequestPeerInfo();
    thenPeerWasNotFound();
}

TEST_F(DiscoveryRegisteredPeerPool, breaks_connection_on_receiving_invalid_info)
{
    givenConnectedPeer();
    whenPeerSendsInvalidInformation();
    thenConnectionIsBroken();
}

TEST_F(DiscoveryRegisteredPeerPool, filters_peer_list_by_type)
{
    givenMultipleOnlinePeersOfDifferentTypes();
    whenRequestPeersOfSpecificType();
    thenOnlyPeersOfThatTypeAreReported();
}

TEST_F(DiscoveryRegisteredPeerPool, filter_by_unknown_type_produces_empty_result)
{
    givenMultipleOnlinePeersOfDifferentTypes();
    whenRequestPeersOfUnknownType();
    thenEmptyListIsReturned();
}

} // namespace test
} // namespace discovery
} // namespace cloud
} // namespace nx
