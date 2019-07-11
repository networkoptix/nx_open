#include <memory>

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/cloud/mediator/listening_peer_pool.h>

#include "server/test_connection.h"

namespace nx {
namespace hpm {
namespace test {

class ListeningPeerPool:
    public ::testing::Test
{
private:
    struct ConnectionContext
    {
        MediaserverData data;
        std::shared_ptr<TestTcpConnection> connection;
        nx::hpm::api::ConnectionSpeed uplinkSpeed;
    };

public:
    ListeningPeerPool()
    {
        m_settings.connectionInactivityTimeout =
            std::chrono::milliseconds(nx::utils::random::number<int>());

        const_cast<conf::ListeningPeerDb&>(
            m_listeningPeerDbSettings.listeningPeerDb()).enabled = true;
        const_cast<clusterdb::map::Settings&>(
            m_listeningPeerDbSettings.listeningPeerDb().map).enableCache = true;

        m_listeningPeerDb =
            std::make_unique<hpm::ListeningPeerDb>(m_listeningPeerDbSettings);
        m_listeningPeerPool =
            std::make_unique<hpm::ListeningPeerPool>(m_settings, m_listeningPeerDb.get());

        // Necessary for choosing server with best connection speed
        m_listeningPeerDb->initialize();
        m_listeningPeerDb->setThisMediatorEndpoint(MediatorEndpoint());
    }

    ~ListeningPeerPool()
    {
        whenCloseAllConnections();
        m_listeningPeerDb->stop();
    }

protected:
    void givenPoolWithConnections()
    {
        whenInsertPeer();
    }

    void givenPoolWithMultipleConnectionsOfDifferentUplinkSpeed()
    {
        for (int i = 0; i < 2; ++i)
            whenInsertPeer();

        findBestConnection();

        for (const auto& connection : m_connections)
            addUplinkSpeed(connection);
    }

    void whenInsertPeer(int bandwidth = 0)
    {
        if (m_systemId.isEmpty())
            m_systemId = QnUuid::createUuid().toSimpleByteArray();

        m_connections.emplace_back(ConnectionContext());
        m_connections.back().connection = std::make_shared<TestTcpConnection>();
        m_connections.back().data.serverId = QnUuid::createUuid().toSimpleByteArray();
        m_connections.back().data.systemId = m_systemId;
        m_connections.back().uplinkSpeed.bandwidth =
            bandwidth
                ? bandwidth
                : nx::utils::random::number(10, 10000);
        m_connections.back().uplinkSpeed.pingTime =
            std::chrono::microseconds(nx::utils::random::number(10, 10000));

        m_listeningPeerPool->insertAndLockPeerData(
            m_connections.back().connection,
            m_connections.back().data);
    }

    void whenCloseAllConnections()
    {
        m_chosenPeer = boost::none;
        for (const auto& connection : m_connections)
        {
            if (connection.connection)
                connection.connection->pleaseStopSync();
        }
    }

    void whenDeletePool()
    {
        m_chosenPeer = boost::none;
        m_listeningPeerPool.reset();
    }

    void whenChooseServerBySystemId()
    {
        m_chosenPeer = m_listeningPeerPool->findAndLockPeerDataByHostName(m_systemId);
        ASSERT_TRUE(m_chosenPeer);
    }

    void whenBestConnectionisClosed()
    {
        m_bestConnection->connection->pleaseStopSync();
        auto data = m_bestConnection->data;
        *m_bestConnection = ConnectionContext();
        m_bestConnection->data = std::move(data);
    }

    void whenInsertPeerWithHigherUplinkSpeed()
    {
        whenInsertPeer(m_bestConnection->uplinkSpeed.bandwidth + 1);
        addUplinkSpeed(m_connections.back());
    }

    void thenBestConnectionIsUpdated()
    {
        findBestConnection();
        whenChooseServerBySystemId();
        thenServerWithBestUplinkSpeedIsChosen();
    }

    void thenServerWithBestUplinkSpeedIsChosen()
    {
        auto chosenPeer = std::move(m_chosenPeer.get());
        ASSERT_EQ(
            m_bestConnection->data.hostName().toLower(),
            chosenPeer.key().hostName().toLower());
    }

    void thenConnectionInactivityTimeoutIsSetProperly()
    {
        for (const auto& connection : m_connections)
        {
            ASSERT_EQ(
                m_settings.connectionInactivityTimeout,
                connection.connection->inactivityTimeout());
        }
    }

    void thenProcessDoesNotCrash()
    {
        // TODO
    }

private:
    void findBestConnection()
    {
        m_bestConnection = nullptr;
        for (auto& connection : m_connections)
        {
            if (!m_bestConnection)
                m_bestConnection = &connection;

            if (m_bestConnection->uplinkSpeed.bandwidth < connection.uplinkSpeed.bandwidth)
                m_bestConnection = &connection;
        }
    }

    void addUplinkSpeed(const ConnectionContext& connection)
    {
        nx::utils::SyncQueue<bool> done;
        m_listeningPeerDb->addUplinkSpeed(
            nx::toStdString(connection.data.hostName()),
            connection.uplinkSpeed,
            [&done](bool success)
            {
                done.push(success);
            });
        ASSERT_TRUE(done.pop());
    }

private:
    conf::ListeningPeer m_settings;
    conf::Settings m_listeningPeerDbSettings;
    std::unique_ptr<hpm::ListeningPeerDb> m_listeningPeerDb;
    std::unique_ptr<hpm::ListeningPeerPool> m_listeningPeerPool;
    std::vector<ConnectionContext> m_connections;
    nx::String m_systemId;
    ConnectionContext* m_bestConnection = nullptr;
    boost::optional<nx::hpm::ListeningPeerPool::ConstDataLocker> m_chosenPeer;
};

TEST_F(ListeningPeerPool, sets_connection_inactivity_timeout)
{
    whenInsertPeer();
    thenConnectionInactivityTimeoutIsSetProperly();
}

TEST_F(
    ListeningPeerPool,
    connection_closed_event_issued_after_ListeningPeerPool_destruction_does_not_crash_process)
{
    givenPoolWithConnections();

    whenDeletePool();
    whenCloseAllConnections();

    thenProcessDoesNotCrash();
}

TEST_F(ListeningPeerPool, chooses_server_with_best_connection_speed)
{
    givenPoolWithMultipleConnectionsOfDifferentUplinkSpeed();

    whenChooseServerBySystemId();

    thenServerWithBestUplinkSpeedIsChosen();
}

TEST_F(ListeningPeerPool, best_uplink_speed_is_updated_after_connection_closure)
{
    givenPoolWithMultipleConnectionsOfDifferentUplinkSpeed();

    whenBestConnectionisClosed();

    thenBestConnectionIsUpdated();
}

TEST_F(ListeningPeerPool, best_uplink_speed_is_updated_when_better_connection_is_added)
{
    givenPoolWithMultipleConnectionsOfDifferentUplinkSpeed();

    whenInsertPeerWithHigherUplinkSpeed();

    thenBestConnectionIsUpdated();
}


} // namespace test
} // namespace hpm
} // namespace nx
