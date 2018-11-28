#include <gtest/gtest.h>

#include <nx/cloud/mediator/test_support/local_cloud_data_provider.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/utils/random.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx::hpm::test {

class StressTest:
    public MediatorFunctionalTest
{
    using base_type = MediatorFunctionalTest;

public:
    ~StressTest()
    {
        while (!m_peers.empty())
        {
            auto& peer = m_peers.back();

            if (peer->serverSocket)
                peer->serverSocket->pleaseStopSync();
            peer->serverSocket.reset();
            
            if (peer->mediatorConnector)
                peer->mediatorConnector->pleaseStopSync();
            peer->mediatorConnector.reset();

            m_peers.pop_back();
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        registerCloudDataProvider(&m_localCloudDataProvider);

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void setUniquePeerCount(int count)
    {
        generateCloudCredentials(count);
        registerAllCredentialsInMediator();
    }

    void setTargetConnectorCountPerPeer(int count)
    {
        m_targetConnectorCountPerPeer = count;
    }

    void start()
    {
        for (int i = 0; i < m_targetConnectorCountPerPeer; ++i)
        {
            for (const auto& credentials: m_credentials)
            {
                auto peer = preparePeer(credentials);
                peer->serverSocket->acceptAsync(
                    [this](auto&&... args) { saveAcceptedSocket(std::move(args)...); });

                m_peers.push_back(std::move(peer));
            }
        }
    }

    void waitForListeningPeerCountToReach(int count)
    {
        for (;;)
        {
            const auto [resultCode, listeningPeers] = getListeningPeers();
            ASSERT_EQ(api::ResultCode::ok, resultCode);
            if ((int) listeningPeers.systems.size() >= count)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    struct Peer
    {
        std::unique_ptr<api::MediatorConnector> mediatorConnector;
        std::unique_ptr<network::cloud::CloudServerSocket> serverSocket;
    };

    int m_targetConnectorCountPerPeer = 1;
    LocalCloudDataProvider m_localCloudDataProvider;
    std::vector<AbstractCloudDataProvider::System> m_credentials;
    std::vector<std::unique_ptr<Peer>> m_peers;

    void generateCloudCredentials(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            AbstractCloudDataProvider::System system;
            system.id = QnUuid::createUuid().toSimpleByteArray();
            system.authKey = QnUuid::createUuid().toSimpleByteArray();
            m_credentials.push_back(system);
        }
    }

    void registerAllCredentialsInMediator()
    {
        for (const auto& credentials: m_credentials)
            m_localCloudDataProvider.addSystem(credentials.id, credentials);
    }

    std::unique_ptr<Peer> preparePeer(const AbstractCloudDataProvider::System& systemInfo)
    {
        auto peer = std::make_unique<Peer>();
        peer->mediatorConnector = std::make_unique<api::MediatorConnector>("cloudHost");

        api::SystemCredentials credentials;
        credentials.systemId = systemInfo.id;
        credentials.serverId = systemInfo.id;
        credentials.key = systemInfo.authKey;
        peer->mediatorConnector->setSystemCredentials(credentials);
        peer->mediatorConnector->mockupMediatorAddress({httpUrl(), stunUdpEndpoint()});
        
        peer->serverSocket = std::make_unique<network::cloud::CloudServerSocket>(
            peer->mediatorConnector.get());

        return peer;
    }

    void saveAcceptedSocket(
        SystemError::ErrorCode /*systemErrorCode*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/)
    {
    }
};

static constexpr int kPeerToEmulateCount = 17;
static constexpr auto kTestDuration = std::chrono::seconds(3);

TEST_F(StressTest, multiple_simultaneous_listening_peers)
{
    setUniquePeerCount(kPeerToEmulateCount);
    
    start();

    waitForListeningPeerCountToReach(kPeerToEmulateCount);
}

TEST_F(StressTest, multiple_blinking_listening_peers)
{
    setUniquePeerCount(kPeerToEmulateCount);
    // Mediator will replace connection from existing peer.
    setTargetConnectorCountPerPeer(3);

    start();

    constexpr auto testDuration = kTestDuration;
    std::this_thread::sleep_for(testDuration);
}

} // namespace nx::hpm::test
