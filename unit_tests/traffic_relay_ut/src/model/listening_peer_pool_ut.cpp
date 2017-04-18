#include <thread>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <model/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {
namespace test {

namespace {

class StreamSocketStub:
    public nx::network::StreamSocketDelegate
{
    using base_type = nx::network::StreamSocketDelegate;

public:
    StreamSocketStub():
        base_type(&m_delegatee)
    {
        setNonBlockingMode(true);
    }

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        post([this, handler = std::move(handler)]() { m_readHandler = std::move(handler); } );
    }

    void setConnectionToClosedState()
    {
        post(
            [this]()
            {
                if (m_readHandler)
                    nx::utils::swapAndCall(m_readHandler, SystemError::noError, 0);
            });
    }

private:
    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    nx::network::TCPSocket m_delegatee;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test fixture.

class ListeningPeerPool:
    public ::testing::Test
{
public:
    ListeningPeerPool():
        m_peerName(nx::utils::generateRandomName(17).toStdString()),
        m_peerConnection(nullptr)
    {
    }

protected:
    void addConnection()
    {
        auto connection = std::make_unique<StreamSocketStub>();
        m_peerConnection = connection.get();
        m_pool.addConnection(m_peerName, std::move(connection));
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

        m_pool.takeIdleConnection(
            m_peerName,
            std::bind(&ListeningPeerPool::onTakeIdleConnectionCompletion, this, _1, _2));
    }

    void whenRequestedConnectionOfUnknownPeer()
    {
        m_peerName = "unknown peer name";
        whenRequestedConnection();
    }

    void assertConnectionHasBeenAdded()
    {
        ASSERT_GT(m_pool.getConnectionCountByPeerName(m_peerName), 0U);
    }

    void assetPeerIsListening()
    {
        ASSERT_TRUE(m_pool.isPeerListening(m_peerName));
    }

    void assetPeerIsNotListening()
    {
        ASSERT_FALSE(m_pool.isPeerListening(m_peerName));
    }

    void thenConnectionIsRemovedFromPool()
    {
        while (m_pool.getConnectionCountByPeerName(m_peerName) > 0U)
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

private:
    struct TakeIdleConnectionResult
    {
        api::ResultCode code;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    model::ListeningPeerPool m_pool;
    std::string m_peerName;
    StreamSocketStub* m_peerConnection;
    nx::utils::SyncQueue<TakeIdleConnectionResult> m_takeIdleConnectionResults;

    void onTakeIdleConnectionCompletion(
        api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
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
    // TODO
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
