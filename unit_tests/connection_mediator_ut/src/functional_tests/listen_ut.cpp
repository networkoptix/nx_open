#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/socket_global.h>

#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>

#include <listening_peer_pool.h>
#include <mediator_service.h>
#include <test_support/mediaserver_emulator.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class ListeningPeer:
    public MediatorFunctionalTest
{
protected:
    void whenStopServer()
    {
        m_mediaServerEmulator.reset();
    }

    void thenSystemDisappearedFromListeningPeerList()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
        data::ListeningPeers listeningPeers;

        for (;;)
        {
            std::tie(statusCode, listeningPeers) = getListeningPeers();
            ASSERT_EQ(nx_http::StatusCode::ok, statusCode);
            if (listeningPeers.systems.find(m_system.id) == listeningPeers.systems.end())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
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
    auto strongConnectionRef = dataLocker->value().peerConnection.lock();
    ASSERT_NE(nullptr, strongConnectionRef);
    ASSERT_EQ(
        server2->mediatorConnectionLocalAddress(),
        strongConnectionRef->getSourceAddress());
    dataLocker.reset();
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

    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    data::ListeningPeers listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx_http::StatusCode::ok, statusCode);
    ASSERT_EQ(1U, listeningPeers.systems.size());

    whenStopServer();
    thenSystemDisappearedFromListeningPeerList();
}

} // namespace test
} // namespace hpm
} // namespace nx
