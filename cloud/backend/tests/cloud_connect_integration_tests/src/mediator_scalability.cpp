#include "basic_test_fixture.h"

#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>

namespace nx::network::cloud::test {

namespace {

static constexpr int kMaxMediatorCount = 3;

} // namespace

class MediatorScalability:
    public BasicTestFixture,
    public testing::Test
{
    using base_type = BasicTestFixture;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        for (int i = mediatorCount(); i < kMaxMediatorCount; ++i)
        {
            addMediator();
            startMediator(i);
        }
    }

    void givenMultipleMediators()
    {
        // Done in SetUp().
    }

    void whenServerConnectsToMediator(int mediatorIndex)
    {
        m_serverConnectionIndex = mediatorIndex;
        startServer(mediatorIndex);
    }

    void givenSynchronizedClusterWithServerListeningOnMediator(int mediatorIndex)
    {
        givenMultipleMediators();
        whenServerConnectsToMediator(mediatorIndex);
        thenServerInfoIsSynchronized();

        // Verifying server connection by successfully connecting the client to server.
        assertClientConnectsToServer();
    }

    void whenClientConnectsToMediator(int mediatorIndex)
    {
        configureGlobalMediatorConnector(mediatorIndex);
    }

    void whenServerConnectionIsBroken()
    {
        stopServer();
        restartMediator(m_serverConnectionIndex);
    }

    void thenServerInfoIsSynchronized()
    {
        m_serverConnectionEndpoint = waitUntilServerIsRegisteredOnMediator();
    }

    void thenClientConnectedToServer()
    {
        waitUntilCloudConnectionCanBeEstablished();
    }

    void andDataIsExchangedBetweenClientAndServer()
    {
        for (int i = 0; i < 3; ++i)
            startExchangingFixedData();

        assertDataHasBeenExchangedCorrectly();
    }

    void andServerConnectionInfoIsUpdated()
    {
        auto expectedEndpoint =
            mediator(m_serverConnectionIndex)
                .moduleInstance()->impl()->listeningPeerDb().thisMediatorEndpoint();
        while (expectedEndpoint != m_serverConnectionEndpoint)
            thenServerInfoIsSynchronized();
    }

private:
    void configureGlobalMediatorConnector(int mediatorIndex)
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            mediatorContext(mediatorIndex).mediator.httpUrl(),
            mediatorContext(mediatorIndex).mediator.stunUdpEndpoint()});
    }

    void assertClientConnectsToServer()
    {
        whenClientConnectsToMediator(m_serverConnectionIndex);
        thenClientConnectedToServer();
        andDataIsExchangedBetweenClientAndServer();
    }

private:
    std::unique_ptr<nx::hpm::api::MediatorClientTcpConnection> m_connection;
    nx::utils::SyncQueue<std::tuple<
        nx::hpm::api::ResultCode,
        nx::hpm::api::ConnectResponse>> m_connectResponseEvents;
    nx::network::stun::TransportHeader m_responseHeader;
    int m_serverConnectionIndex = -1;
    nx::hpm::MediatorEndpoint m_serverConnectionEndpoint;
};

TEST_F(MediatorScalability, server_info_synchronizes_in_cluster)
{
    givenMultipleMediators();

    whenServerConnectsToMediator(1);

    thenServerInfoIsSynchronized();
}

TEST_F(MediatorScalability, connect_request_is_redirected)
{
    givenSynchronizedClusterWithServerListeningOnMediator(1);

    whenClientConnectsToMediator(0);

    thenClientConnectedToServer();

    andDataIsExchangedBetweenClientAndServer();
}

TEST_F(MediatorScalability, mediator_info_updated_in_cluster_after_server_reconnects)
{
    givenSynchronizedClusterWithServerListeningOnMediator(1);

    whenServerConnectionIsBroken();
    whenServerConnectsToMediator(2);
    whenClientConnectsToMediator(0);

    thenClientConnectedToServer();

    andDataIsExchangedBetweenClientAndServer();
    andServerConnectionInfoIsUpdated();
}

} // nx::network::cloud::test