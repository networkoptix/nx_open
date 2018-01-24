#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/cloud/mediator_server_connections.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/sync_queue.h>

#include <listening_peer_pool.h>
#include <mediator_service.h>
#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class ListeningPeer:
    public MediatorFunctionalTest,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    ListeningPeer():
        m_serverId(QnUuid::createUuid().toSimpleByteArray())
    {
    }

    ~ListeningPeer()
    {
        for (auto& connection: m_serverConnections)
            connection->pleaseStopSync();
    }

    virtual boost::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = m_system.id;
        systemCredentials.serverId = m_serverId;
        systemCredentials.key = m_system.authKey;
        return systemCredentials;
    }

protected:
    void givenListeningServer()
    {
        establishListeningConnectionToMediator();
    }

    void whenStopServer()
    {
        m_mediaServerEmulator.reset();
    }

    void whenServerEstablishesNewConnection()
    {
        establishListeningConnectionToMediator();
    }

    void thenSystemDisappearedFromListeningPeerList()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
        data::ListeningPeers listeningPeers;

        for (;;)
        {
            std::tie(statusCode, listeningPeers) = getListeningPeers();
            ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);
            if (listeningPeers.systems.find(m_system.id) == listeningPeers.systems.end())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thenOldConnectionIsClosedByMediator()
    {
        auto closedConnection = m_closeConnectionEvents.pop();
        ASSERT_EQ(closedConnection, m_serverConnections.front().get());
    }

    nx::hpm::AbstractCloudDataProvider::System& system()
    {
        return m_system;
    }

    const nx::hpm::AbstractCloudDataProvider::System& system() const
    {
        return m_system;
    }

    nx::String serverId() const
    {
        return m_mediaServerEmulator->serverId();
    }

    nx::String fullServerName() const
    {
        return m_mediaServerEmulator->serverId() + "." + m_system.id;
    }

private:
    nx::hpm::AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_mediaServerEmulator;
    nx::String m_serverId;
    std::vector<std::shared_ptr<nx::network::stun::AsyncClient>> m_serverConnections;
    nx::utils::SyncQueue<nx::network::stun::AsyncClient*> m_closeConnectionEvents;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_system = addRandomSystem();
        m_mediaServerEmulator = addRandomServer(system(), boost::none);
        ASSERT_NE(nullptr, m_mediaServerEmulator);
        ASSERT_EQ(
            nx::hpm::api::ResultCode::ok,
            m_mediaServerEmulator->listen().first);
    }

    void establishListeningConnectionToMediator()
    {
        using namespace std::placeholders;

        auto stunClient = std::make_shared<nx::network::stun::AsyncClient>();
        stunClient->connect(nx::network::url::Builder()
            .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(stunEndpoint()));
        stunClient->setOnConnectionClosedHandler(
            std::bind(&ListeningPeer::saveConnectionClosedEvent, this, stunClient.get(), _1));
        auto connection = std::make_unique<nx::hpm::api::MediatorServerTcpConnection>(
            stunClient,
            this);
        auto connectionGuard = makeScopeGuard(
            [&connection]() { connection->pleaseStopSync(); });

        nx::hpm::api::ListenRequest request;
        request.systemId = m_system.id;
        request.serverId = m_serverId;
        nx::utils::promise<
            std::tuple<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>> done;
        connection->listen(
            request,
            [&done](
                nx::hpm::api::ResultCode resultCode,
                nx::hpm::api::ListenResponse response)
            {
                done.set_value(std::make_tuple(resultCode, std::move(response)));
            });
        const auto result = done.get_future().get();
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, std::get<0>(result));

        m_serverConnections.push_back(std::move(stunClient));
    }

    void saveConnectionClosedEvent(
        nx::network::stun::AsyncClient* stunClient,
        SystemError::ErrorCode /*systemErrorCode*/)
    {
        m_closeConnectionEvents.push(stunClient);
    }
};

TEST_F(ListeningPeer, connection_override)
{
    using namespace nx::hpm;

    auto server2 = addServer(system(), serverId());
    ASSERT_NE(nullptr, server2);

    ASSERT_EQ(nx::hpm::api::ResultCode::ok, server2->listen().first);

    // TODO #ak Checking that server2 connection has overridden server1
        // since both servers have same server id.

    auto dataLocker = moduleInstance()->impl()->listeningPeerPool()->
        findAndLockPeerDataByHostName(fullServerName());
    ASSERT_TRUE(static_cast<bool>(dataLocker));
    auto strongConnectionRef = dataLocker->value().peerConnection;
    ASSERT_NE(nullptr, strongConnectionRef);
    ASSERT_EQ(
        server2->mediatorConnectionLocalAddress(),
        strongConnectionRef->getSourceAddress());
    dataLocker.reset();
}

TEST_F(ListeningPeer, connection_is_closed_after_overriding)
{
    givenListeningServer();
    whenServerEstablishesNewConnection();
    thenOldConnectionIsClosedByMediator();
}

TEST_F(ListeningPeer, unknown_system_credentials)
{
    using namespace nx::hpm;

    auto system2 = addRandomSystem();
    system2.authKey.clear();    //< Making credentials invalid.
    auto server2 = addRandomServer(system2, boost::none, hpm::ServerTweak::noBindEndpoint);
    ASSERT_NE(nullptr, server2);
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->bind());
    ASSERT_EQ(nx::hpm::api::ResultCode::notAuthorized, server2->listen().first);
}

TEST_F(ListeningPeer, peer_disconnect)
{
    using namespace nx::hpm;

    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
    api::ListeningPeers listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);
    ASSERT_EQ(1U, listeningPeers.systems.size());

    whenStopServer();
    thenSystemDisappearedFromListeningPeerList();
}

} // namespace test
} // namespace hpm
} // namespace nx
