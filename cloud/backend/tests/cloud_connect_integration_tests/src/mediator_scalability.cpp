#include "basic_test_fixture.h"

#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>

namespace nx::network::cloud::test {

static constexpr int kMaxMediatorCount = 2;

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
            startMediator(mediatorCount() - 1);
        }
    }

    void givenMultipleMediators()
    {
        // Done in SetUp().
    }

    void whenStartServer()
    {
        // Forcing connection to mediator 0.
        startServer(0);
    }

    void givenSynchronizedClusterWithServer()
    {
        givenMultipleMediators();
        whenStartServer();
        thenServerInfoIsSynchronized();
    }

    void whenConnectToServerOnDifferentMediator()
    {
        // Configuring global mediator connector with info for mediator 1
        configureGlobalMediatorConnector(1);
    }

    void thenServerInfoIsSynchronized()
    {
        waitUntilServerIsRegisteredOnMediator();
    }

    void thenConnectionSucceeded()
    {
        assertCloudConnectionCanBeEstablished();
    }

    void andDataIsExchangedBetweenClientAndServer()
    {
        for (int i = 0; i < 3; ++i)
            startExchangingFixedData();

        assertDataHasBeenExchangedCorrectly();
    }

private:
    void configureGlobalMediatorConnector(int mediatorIndex)
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            mediatorContext(mediatorIndex).mediator.httpUrl(),
            mediatorContext(mediatorIndex).mediator.stunUdpEndpoint()});
    }

private:
    std::unique_ptr<nx::hpm::api::MediatorClientTcpConnection> m_connection;
    nx::utils::SyncQueue<std::tuple<
        nx::hpm::api::ResultCode,
        nx::hpm::api::ConnectResponse>> m_connectResponseEvents;
    nx::network::stun::TransportHeader m_responseHeader;
};

TEST_F(MediatorScalability, server_info_synchronizes_in_cluster)
{
    givenMultipleMediators();

    whenStartServer();

    thenServerInfoIsSynchronized();
}

TEST_F(MediatorScalability, connect_request_is_redirected)
{
    givenSynchronizedClusterWithServer();

    whenConnectToServerOnDifferentMediator();

    thenConnectionSucceeded();

    andDataIsExchangedBetweenClientAndServer();
}

} // nx::network::cloud::test