#include <memory>

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

#include <listening_peer_pool.h>
#include <server/test_connection.h>

namespace nx {
namespace hpm {
namespace test {

class ListeningPeerPool:
    public ::testing::Test
{
public:
    ListeningPeerPool()
    {
        m_settings.connectionInactivityTimeout =
            std::chrono::milliseconds(nx::utils::random::number<int>());
        m_listeningPeerPool = std::make_unique<hpm::ListeningPeerPool>(m_settings);

        m_connection = std::make_shared<TestTcpConnection>();
    }

protected:
    void whenInsertPeer()
    {
        m_listeningPeerPool->insertAndLockPeerData(
            m_connection,
            MediaserverData());
    }

    void thenConnectionInactivityTimeoutIsSetProperly()
    {
        ASSERT_EQ(
            m_settings.connectionInactivityTimeout,
            m_connection->inactivityTimeout());
    }

private:
    conf::ListeningPeer m_settings;
    std::unique_ptr<hpm::ListeningPeerPool> m_listeningPeerPool;
    std::shared_ptr<TestTcpConnection> m_connection;
};

TEST_F(ListeningPeerPool, sets_connection_inactivity_timeout)
{
    whenInsertPeer();
    thenConnectionInactivityTimeoutIsSetProperly();
}

} // namespace test
} // namespace hpm
} // namespace nx
