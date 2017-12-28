#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/random.h>
#include <nx/utils/uuid.h>
#include <nx/utils/sync_call.h>

#include <discovery/registered_peer_pool.h>

#include "discovery_test_setup.h"

namespace nx {
namespace cloud {
namespace discovery {
namespace test {

class DiscoveryRegisteredPeerPool:
    public DiscoveryTestSetup
{
public:
    DiscoveryRegisteredPeerPool():
        m_registeredPeerPool(m_configuration)
    {
    }

    ~DiscoveryRegisteredPeerPool()
    {
        for (auto& peerContext: m_peers)
        {
            if (peerContext.connection)
                peerContext.connection->pleaseStopSync();
        }
    }

protected:
    void givenConnectedPeer()
    {
        PeerContext peerContext;

        peerContext.id = QnUuid::createUuid().toStdString();
        nx::network::http::HttpClient httpClient;
        ASSERT_TRUE(httpClient.doUpgrade(getUrl(), nx::network::websocket::kWebsocketProtocolName));
        ASSERT_TRUE(nx::network::http::StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
            << httpClient.response()->statusLine.statusCode;

        auto streamSocket = httpClient.takeSocket();
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));
        auto peerConnection = std::make_unique<nx::network::WebSocket>(std::move(streamSocket));
        m_registeredPeerPool.addPeerConnection(
            peerContext.id,
            std::move(peerConnection));

        m_peers.push_back(std::move(peerContext));
    }

    void givenReportedPeer()
    {
        givenConnectedPeer();
        whenPeerSendsInformation();
    }

    void givenMultipleOnlinePeersOfDifferentTypes()
    {
        const int peerCount = 7;
        for (int i = 0; i < peerCount; ++i)
        {
            givenConnectedPeer();
            m_peers.back().type = nx::utils::generateRandomName(7).toStdString();
            whenPeerSendsInformation();
            thenPeerMovedToOnlineState();
        }
    }

    void whenRequestPeerWithRandomId()
    {
        PeerContext peerContext;
        peerContext.id = nx::utils::generateRandomName(7).toStdString();
        m_peers.push_back(std::move(peerContext));
        whenRequestPeerInfo();
    }

    void whenRequestPeerInfo()
    {
        auto foundPeer = m_registeredPeerPool.getPeerInfo(m_peers[0].id);
        if (foundPeer)
            m_foundPeers.push_back(std::move(*foundPeer));
    }

    void whenPeerConnectionIsBroken()
    {
        m_peers[0].connection->pleaseStopSync();
        m_peers[0].connection.reset();
    }

    void whenPeerSendsInformation()
    {
        PeerInformation peerInfo;
        peerInfo.id = m_peers.back().id;
        peerInfo.apiUrl = "http://nxvms.com/test_peer/";
        if (m_peers.back().type.empty())
            m_peers.back().type = peerInfo.type; //< Using default type.
        else
            peerInfo.type = m_peers.back().type;
        m_peers.back().expectedPeerInfo = QJson::serialized(peerInfo);
        sendPeerInfo(m_peers.back().expectedPeerInfo);
    }

    void whenPeerSendsInvalidInformation()
    {
        sendPeerInfo("invalid json");
    }

    void whenRequestPeersOfSpecificType()
    {
        m_selectedPeerType = nx::utils::random::choice(m_peers).type;
        m_foundPeers = m_registeredPeerPool.getPeerInfoListByType(
            m_selectedPeerType);
    }

    void whenRequestPeersOfUnknownType()
    {
        m_foundPeers = m_registeredPeerPool.getPeerInfoListByType(
            nx::utils::generateRandomName(7).toStdString());
    }

    void thenPeerWasNotFound()
    {
        ASSERT_TRUE(m_foundPeers.empty());
    }

    void thenPeerIsFound()
    {
        ASSERT_FALSE(m_foundPeers.empty());
        // TODO Verifying peer information.
    }

    void thenPeerMovedToOnlineState()
    {
        while (!m_registeredPeerPool.getPeerInfo(m_peers.back().id))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeerMovedToOfflineState()
    {
        while (m_registeredPeerPool.getPeerInfo(m_peers[0].id))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenPeerInformationIsUpdated()
    {
        for (;;)
        {
            auto info = m_registeredPeerPool.getPeerInfo(m_peers[0].id);
            if (info && *info == m_peers[0].expectedPeerInfo.toStdString())
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
                    m_peers[0].connection.get(),
                    &readBuf,
                    std::placeholders::_1));

            if ((systemErrorCode == SystemError::noError && bytesRead == 0) ||
                nx::network::socketCannotRecoverFromError(systemErrorCode))
            {
                break;
            }
        }
    }

    void thenOnlyPeersOfThatTypeAreReported()
    {
        std::vector<BasicInstanceInformation> foundPeerParsedInfo;
        for (const auto& peerJson: m_foundPeers)
        {
            BasicInstanceInformation peerInfo("invalid_type");
            ASSERT_TRUE(QJson::deserialize(QByteArray(peerJson.c_str()), &peerInfo));
            ASSERT_EQ(m_selectedPeerType, peerInfo.type);
            foundPeerParsedInfo.push_back(std::move(peerInfo));
        }

        for (const auto& peerContext: m_peers)
        {
            if (peerContext.type != m_selectedPeerType)
                continue;

            auto peerIter = std::find_if(
                foundPeerParsedInfo.begin(),
                foundPeerParsedInfo.end(),
                [id = peerContext.id](const BasicInstanceInformation& peerInfo)
                {
                    return peerInfo.id == id;
                });
            ASSERT_NE(foundPeerParsedInfo.end(), peerIter);
        }
    }

    void thenEmptyListIsReturned()
    {
        ASSERT_TRUE(m_foundPeers.empty());
    }

private:
    struct PeerContext
    {
        std::string id;
        std::unique_ptr<nx::network::WebSocket> connection;
        nx::Buffer expectedPeerInfo;
        std::string type;
    };

    conf::Discovery m_configuration;
    discovery::RegisteredPeerPool m_registeredPeerPool;
    std::vector<std::string> m_foundPeers;
    std::vector<PeerContext> m_peers;
    std::string m_selectedPeerType;

    void sendPeerInfo(const nx::Buffer& peerInfoJson)
    {
        m_peers.back().connection = m_acceptedConnections.pop();

        nx::utils::promise<SystemError::ErrorCode> sent;
        m_peers.back().connection->sendAsync(
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
