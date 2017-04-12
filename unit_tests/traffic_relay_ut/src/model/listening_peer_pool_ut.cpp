#include <thread>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>

#include <model/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {
namespace test {

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

    void assertConnectionHasBeenAdded()
    {
        ASSERT_GT(m_pool.getConnectionCountByPeerName(m_peerName), 0U);
    }

    void thenConnectionIsRemovedFromPool()
    {
        while (m_pool.getConnectionCountByPeerName(m_peerName) > 0U)
            std::this_thread::yield();
    }

private:
    model::ListeningPeerPool m_pool;
    const std::string m_peerName;
    StreamSocketStub* m_peerConnection;
};

TEST_F(ListeningPeerPool, adding_peer_connection)
{
    addConnection();
    assertConnectionHasBeenAdded();
}

TEST_F(ListeningPeerPool, forgets_tcp_connection_when_it_is_closed)
{
    givenConnectionFromPeer();
    whenConnectionIsClosed();
    thenConnectionIsRemovedFromPool();
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
