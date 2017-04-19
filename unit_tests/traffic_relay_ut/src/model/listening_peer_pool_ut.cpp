#include <thread>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <model/listening_peer_pool.h>

#include "../stream_socket_stub.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {
namespace test {

//-------------------------------------------------------------------------------------------------
// Test fixture.

class ListeningPeerPool:
    public ::testing::Test
{
public:
    ListeningPeerPool():
        m_peerName(nx::utils::generateRandomName(17).toStdString()),
        m_peerConnection(nullptr),
        m_poolHasBeenDestroyed(false)
    {
        m_pool = std::make_unique<model::ListeningPeerPool>();
    }

protected:
    void addConnection()
    {
        auto connection = std::make_unique<StreamSocketStub>();
        connection->bindToAioThread(
            network::SocketGlobals::aioService().getRandomAioThread());
        m_peerConnection = connection.get();
        m_pool->addConnection(m_peerName, std::move(connection));
    }

    void givenConnectionFromPeer()
    {
        addConnection();
    }

    void whenConnectionIsClosed()
    {
        m_peerConnection->setConnectionToClosedState();
    }

    void whenRequestedConnection()
    {
        using namespace std::placeholders;

        m_pool->takeIdleConnection(
            m_peerName,
            std::bind(&ListeningPeerPool::onTakeIdleConnectionCompletion, this, _1, _2));
    }

    void whenRequestedConnectionOfUnknownPeer()
    {
        m_peerName = "unknown peer name";
        whenRequestedConnection();
    }

    void whenFreedListeningPeerPool()
    {
        m_pool.reset();
        m_poolHasBeenDestroyed = true;
    }

    void assertConnectionHasBeenAdded()
    {
        ASSERT_GT(m_pool->getConnectionCountByPeerName(m_peerName), 0U);
    }

    void assetPeerIsListening()
    {
        ASSERT_TRUE(m_pool->isPeerListening(m_peerName));
    }

    void assetPeerIsNotListening()
    {
        ASSERT_FALSE(m_pool->isPeerListening(m_peerName));
    }

    void thenConnectionIsRemovedFromPool()
    {
        while (m_pool->getConnectionCountByPeerName(m_peerName) > 0U)
            std::this_thread::yield();
    }

    void thenConnectionHasBeenProvided()
    {
        const auto result = m_takeIdleConnectionResults.pop();
        ASSERT_EQ(api::ResultCode::ok, result.code);
        ASSERT_NE(nullptr, result.connection);
    }

    void thenNoConnectionHasBeenProvided()
    {
        const auto result = m_takeIdleConnectionResults.pop();
        ASSERT_NE(api::ResultCode::ok, result.code);
        ASSERT_EQ(nullptr, result.connection);
    }

    void thenConnectRequestHasCompleted()
    {
        m_takeIdleConnectionResults.pop();
    }

private:
    struct TakeIdleConnectionResult
    {
        api::ResultCode code;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    std::unique_ptr<model::ListeningPeerPool> m_pool;
    std::atomic<bool> m_poolHasBeenDestroyed;
    std::string m_peerName;
    StreamSocketStub* m_peerConnection;
    nx::utils::SyncQueue<TakeIdleConnectionResult> m_takeIdleConnectionResults;

    void onTakeIdleConnectionCompletion(
        api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ASSERT_FALSE(m_poolHasBeenDestroyed.load());
        m_takeIdleConnectionResults.push({resultCode, std::move(connection)});
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(ListeningPeerPool, adding_peer_connection)
{
    addConnection();
    assertConnectionHasBeenAdded();
    assetPeerIsListening();
}

TEST_F(ListeningPeerPool, forgets_tcp_connection_when_it_is_closed)
{
    givenConnectionFromPeer();
    whenConnectionIsClosed();
    thenConnectionIsRemovedFromPool();
    assetPeerIsNotListening();
}

TEST_F(ListeningPeerPool, get_idle_connection)
{
    givenConnectionFromPeer();
    whenRequestedConnection();
    thenConnectionHasBeenProvided();
}

TEST_F(ListeningPeerPool, get_idle_connection_for_unknown_peer)
{
    whenRequestedConnectionOfUnknownPeer();
    thenNoConnectionHasBeenProvided();
}

TEST_F(ListeningPeerPool, waits_get_idle_connection_completion_before_destruction)
{
    givenConnectionFromPeer();

    whenRequestedConnection();
    whenFreedListeningPeerPool();

    thenConnectRequestHasCompleted();
}

TEST_F(
    ListeningPeerPool,
    connection_closes_simultaneously_with_take_connection_request)
{
    for (int i = 0; i < 100; ++i)
    {
        givenConnectionFromPeer();
        whenConnectionIsClosed();
        whenRequestedConnection();
        thenConnectRequestHasCompleted();
    }
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
