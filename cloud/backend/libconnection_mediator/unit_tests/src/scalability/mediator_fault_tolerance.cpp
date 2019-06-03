#include "mediator_scalability_test_fixture.h"

#include <nx/network/stun/async_client.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>

namespace nx::hpm::test {

class MediatorFaultTolerance : public MediatorScalabilityTestFixture
{
protected:
    void whenServerConnectsToAllMediators()
    {
        // Adding a server to mediator 0.
        whenAddServer();
        // Emulating the same server connecting to mediator 1.
        emulateServerConnectionToAnotherMediator();
    }

    void thenServerIsConnectedToAllMediators()
    {
        // Verify that all mediators to which server is connected will be selected by the
        // ListeningPeerDb, eventually.

        std::vector<nx::hpm::MediatorEndpoint> expectedEndpoints = m_mediatorCluster->endpoints();

        std::mutex mutex;
        std::vector<nx::hpm::MediatorEndpoint> selectedEndpoints;

        const auto haveEndpoint =
            [&mutex, &selectedEndpoints](
                const nx::hpm::MediatorEndpoint& targetEndpoint) -> bool
        {
            std::lock_guard lock(mutex);
            return std::find_if(
                selectedEndpoints.begin(),
                selectedEndpoints.end(),
                [&targetEndpoint] (const MediatorEndpoint& endpoint)
                {
                    return targetEndpoint == endpoint;
                }) != selectedEndpoints.end();
        };

        for (const auto& expectedEndpoint : expectedEndpoints)
        {
            while (!haveEndpoint(expectedEndpoint))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                auto selectedEndpoint =
                    m_mediatorCluster->lookupMediatorEndpoint(m_mediaServerFullName);
                if (!selectedEndpoint)
                    continue;

                if (!haveEndpoint(*selectedEndpoint))
                {
                    std::lock_guard lock(mutex);
                    selectedEndpoints.push_back(*selectedEndpoint);
                }
            }
        }
    }

    void andClientCanConnectToServerViaAllMediators()
    {
        using namespace nx::network;

        std::vector<MediaServerEmulator*> servers = { m_mediaServer.get(), m_mediaServer2.get() };
        ASSERT_TRUE(m_mediatorCluster->size() >= (int)servers.size());

        for (auto i = 0; i < servers.size(); ++i)
        {
            auto udpClient =
                std::make_unique<api::MediatorClientUdpConnection>(
                    m_mediatorCluster->mediator(i).stunUdpEndpoint());

            api::ConnectRequest request;
            request.connectionMethods = api::ConnectionMethod::udpHolePunching;
            request.connectSessionId = QnUuid::createUuid().toSimpleByteArray();
            request.destinationHostName = m_mediaServerFullName.c_str();
            request.originatingPeerId = QnUuid::createUuid().toSimpleByteArray();

            nx::utils::SyncQueue <std::tuple<
                stun::TransportHeader,
                nx::hpm::api::ResultCode,
                nx::hpm::api::ConnectResponse>> connectResponseEvent;

            udpClient->connect(
                request,
                [&connectResponseEvent](
                    stun::TransportHeader stunTransportHeader,
                    nx::hpm::api::ResultCode resultCode,
                    nx::hpm::api::ConnectResponse response)
                {
                    connectResponseEvent.push(
                        std::make_tuple(stunTransportHeader, resultCode, std::move(response)));
                });

            auto [header, resultCode, response] = connectResponseEvent.pop();
            ASSERT_EQ(nx::hpm::api::ResultCode::ok, resultCode);
            ASSERT_EQ(
                m_mediatorCluster->mediator(i).stunUdpEndpoint().toStdString(),
                header.locationEndpoint.toStdString());

            udpClient->pleaseStopSync();
        }
    }

private:
    /**
     * Mediaserver doesn't support connection to multiple mediators yet,
     * so we emulate it by starting a second server and connecting to a second mediator
     * with the same server id as the first server.
     */
    void emulateServerConnectionToAnotherMediator()
    {
        auto& mediator = m_mediatorCluster->mediator(1);
        m_mediaServer2 = mediator.addServer(m_system, m_mediaServer->serverId());

        auto [resultCode, response] = m_mediaServer2->listen();
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, resultCode);
    }

private:
    std::unique_ptr<MediaServerEmulator> m_mediaServer2;
};

TEST_F(MediatorFaultTolerance, multiple_mediators_are_selected)
{
    givenMultipleMediators();

    whenServerConnectsToAllMediators();

    thenServerIsConnectedToAllMediators();

    andClientCanConnectToServerViaAllMediators();
}

}