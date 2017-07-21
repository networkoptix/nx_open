#include <map>
#include <thread>
#include <string>

#include <gtest/gtest.h>

#include <nx/network/socket_delegate.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <model/listening_peer_pool.h>
#include <settings.h>

#include "../settings_loader.h"
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
        m_poolHasBeenDestroyed(false),
        m_peerName(nx::utils::generateRandomName(17).toStdString()),
        m_peerConnection(nullptr)
    {
        addArg("-listeningPeer/disconnectedPeerTimeout", "1ms");
        addArg("-listeningPeer/takeIdleConnectionTimeout", "1ms");
        addArg("-listeningPeer/internalTimerPeriod", "1ms");
    }

protected:
    void addArg(const std::string& name, const std::string& value)
    {
        m_settingsLoader.addArg(name, value);
    }

    void addConnection()
    {
        addConnection(m_peerName);
    }

    void addConnection(const std::string& peerName)
    {
        auto connection = std::make_unique<relay::test::StreamSocketStub>();
        connection->setPostDelay(m_connectionPostDelay);
        connection->bindToAioThread(
            network::SocketGlobals::aioService().getRandomAioThread());
        m_peerConnection = connection.get();
        m_peerConnections.push_back(connection.get());
        pool().addConnection(peerName, std::move(connection));
    }

    void givenConnectionFromPeer()
    {
        addConnection(m_peerName);
        m_peerConnectedEvents.pop();
    }

    void givenListeningPeerWhoseConnectionsHaveBeenTaken()
    {
        givenConnectionFromPeer();
        whenRequestedConnection();
        thenConnectionHasBeenProvided();
    }

    void givenPeerWithMultipleConnections()
    {
        for (int i= 0; i < 3; ++i)
            addConnection(m_peerName);
    }

    void whenPeerHasEstablshedNewConnection()
    {
        addConnection(m_peerName);
    }

    void whenConnectionIsClosed()
    {
        m_peerConnection->setConnectionToClosedState();
    }

    void whenRequestedConnection()
    {
        using namespace std::placeholders;

        pool().takeIdleConnection(
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

    void whenAddConnectionToThePool()
    {
        addConnection(m_peerName);
    }

    void whenCloseAllConnections()
    {
        for (std::size_t i = 0; i < m_peerConnections.size(); ++i)
            m_peerConnections[i]->setConnectionToClosedState();
    }

    void whenTakenConnectionFromPeer()
    {
        whenRequestedConnection();
        thenConnectionHasBeenProvided();
    }

    void assertConnectionHasBeenAdded()
    {
        ASSERT_GT(pool().getConnectionCountByPeerName(m_peerName), 0U);
    }

    void assertPeerIsOnline()
    {
        ASSERT_TRUE(pool().isPeerOnline(m_peerName));
    }

    void assertPeerIsOffline()
    {
        ASSERT_FALSE(pool().isPeerOnline(m_peerName));
    }

    void thenPeerBecomesOffline()
    {
        while (pool().isPeerOnline(m_peerName))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void thenConnectionIsRemovedFromPool()
    {
        while (pool().getConnectionCountByPeerName(m_peerName) > 0U)
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

    void thenGetConnectionRequestIs(api::ResultCode expectedResultCode)
    {
        const auto result = m_takeIdleConnectionResults.pop();
        ASSERT_EQ(expectedResultCode, result.code);
        if (expectedResultCode == api::ResultCode::ok)
            ASSERT_NE(nullptr, result.connection);
        else
            ASSERT_EQ(nullptr, result.connection);
    }

    void thenConnectRequestHasCompleted()
    {
        m_takeIdleConnectionResults.pop();
    }

    void thenKeepAliveHasBeenEnabledOnConnection()
    {
        boost::optional<KeepAliveOptions> keepAliveOptions;
        ASSERT_TRUE(m_peerConnection->getKeepAlive(&keepAliveOptions));
        ASSERT_TRUE(static_cast<bool>(keepAliveOptions));
        ASSERT_EQ(
            m_settingsLoader.settings().listeningPeer().tcpKeepAlive,
            *keepAliveOptions);
    }

    void thenPeerConnectedEventHasBeenRaised()
    {
        ASSERT_EQ(m_peerName, m_peerConnectedEvents.pop());
    }

    void thenPeerConnectedEventHasNotBeenRaised()
    {
        ASSERT_TRUE(m_peerConnectedEvents.isEmpty());
    }

    void thenPeerDisconnectedEventHasBeenRaised()
    {
        ASSERT_EQ(m_peerName, m_peerDisconnectedEvents.pop());
    }

    void thenPeerDisconnectedEventHasNotBeenRaised()
    {
        ASSERT_TRUE(m_peerDisconnectedEvents.isEmpty());
    }

    const model::ListeningPeerPool& pool() const
    {
        if (!m_pool)
            const_cast<ListeningPeerPool*>(this)->initializePool();
        return *m_pool;
    }

    model::ListeningPeerPool& pool()
    {
        if (!m_pool)
            initializePool();
        return *m_pool;
    }

    void setPeerName(const std::string& peerName)
    {
        m_peerName = peerName;
    }

    void setConnectionPostDelay(std::chrono::milliseconds postDelay)
    {
        m_connectionPostDelay = postDelay;
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
    relay::test::StreamSocketStub* m_peerConnection;
    std::vector<relay::test::StreamSocketStub*> m_peerConnections;
    nx::utils::SyncQueue<TakeIdleConnectionResult> m_takeIdleConnectionResults;
    SettingsLoader m_settingsLoader;
    boost::optional<std::chrono::milliseconds> m_connectionPostDelay;
    nx::utils::SyncQueue<std::string> m_peerConnectedEvents;
    nx::utils::SyncQueue<std::string> m_peerDisconnectedEvents;

    void onTakeIdleConnectionCompletion(
        api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ASSERT_FALSE(m_poolHasBeenDestroyed.load());
        m_takeIdleConnectionResults.push({resultCode, std::move(connection)});
    }

    void initializePool()
    {
        using namespace std::placeholders;

        m_settingsLoader.load();
        m_pool = std::make_unique<model::ListeningPeerPool>(
            m_settingsLoader.settings());

        nx::utils::SubscriptionId subscriptionId = nx::utils::kInvalidSubscriptionId;
        m_pool->peerConnectedSubscription().subscribe(
            std::bind(&nx::utils::SyncQueue<std::string>::push, &m_peerConnectedEvents, _1),
            &subscriptionId);

        m_pool->peerDisconnectedSubscription().subscribe(
            std::bind(&nx::utils::SyncQueue<std::string>::push, &m_peerDisconnectedEvents, _1),
            &subscriptionId);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(ListeningPeerPool, adding_peer_connection)
{
    addConnection();
    assertConnectionHasBeenAdded();
    assertPeerIsOnline();
}

TEST_F(ListeningPeerPool, forgets_tcp_connection_when_it_is_closed)
{
    givenConnectionFromPeer();
    whenConnectionIsClosed();
    thenConnectionIsRemovedFromPool();
}

TEST_F(ListeningPeerPool, peer_with_idle_connections_is_online)
{
    givenConnectionFromPeer();
    assertPeerIsOnline();
}

TEST_F(
    ListeningPeerPool,
    peer_without_idle_connections_is_online_before_expiration_timeout_passes)
{
    addArg("-listeningPeer/disconnectedPeerTimeout", "1m");

    givenListeningPeerWhoseConnectionsHaveBeenTaken();
    assertPeerIsOnline();
}

TEST_F(
    ListeningPeerPool,
    peer_without_idle_connections_becomes_offline_when_timeout_passes)
{
    givenListeningPeerWhoseConnectionsHaveBeenTaken();
    thenPeerBecomesOffline();
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

TEST_F(ListeningPeerPool, get_idle_connection_waits_for_connection_to_appear)
{
    addArg("-listeningPeer/disconnectedPeerTimeout", "1m");
    addArg("-listeningPeer/takeIdleConnectionTimeout", "1m");

    givenListeningPeerWhoseConnectionsHaveBeenTaken();
    whenRequestedConnection();
    whenPeerHasEstablshedNewConnection();
    thenConnectionHasBeenProvided();
}

TEST_F(
    ListeningPeerPool,
    get_idle_connection_times_out_if_no_new_connection_established)
{
    addArg("-listeningPeer/disconnectedPeerTimeout", "1m");

    givenListeningPeerWhoseConnectionsHaveBeenTaken();
    whenRequestedConnection();
    thenGetConnectionRequestIs(api::ResultCode::timedOut);
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
    for (int i = 0; i < 17; ++i)
    {
        givenConnectionFromPeer();
        whenConnectionIsClosed();
        whenRequestedConnection();
        thenConnectRequestHasCompleted();
    }
}

TEST_F(ListeningPeerPool, enables_tcp_keep_alive)
{
    whenAddConnectionToThePool();
    thenKeepAliveHasBeenEnabledOnConnection();
}

TEST_F(ListeningPeerPool, connection_closed_simultaneously_with_take_request)
{
    setConnectionPostDelay(std::chrono::milliseconds(17));

    givenConnectionFromPeer();

    whenRequestedConnection();
    whenCloseAllConnections();

    thenConnectRequestHasCompleted();
}

TEST_F(ListeningPeerPool, event_peer_connected_reported)
{
    whenAddConnectionToThePool();
    thenPeerConnectedEventHasBeenRaised();
}

TEST_F(ListeningPeerPool, event_peer_connected_not_reported_on_adding_subsequent_connection)
{
    givenConnectionFromPeer();
    whenAddConnectionToThePool();
    thenPeerConnectedEventHasNotBeenRaised();
}

TEST_F(ListeningPeerPool, event_peer_disconnected_reported)
{
    givenListeningPeerWhoseConnectionsHaveBeenTaken();
    thenPeerDisconnectedEventHasBeenRaised();
}

TEST_F(ListeningPeerPool, event_peer_disconnected_not_reported_on_taking_last_connection)
{
    givenPeerWithMultipleConnections();
    whenTakenConnectionFromPeer();
    thenPeerDisconnectedEventHasNotBeenRaised();
}

//-------------------------------------------------------------------------------------------------

class ListeningPeerPoolFindPeerByParentDomainName:
    public ListeningPeerPool
{
public:
    ListeningPeerPoolFindPeerByParentDomainName():
        m_domainName(nx::utils::generateRandomName(17).toStdString())
    {
        m_peerNames.resize(7);
        for (auto& peerName: m_peerNames)
            peerName = nx::utils::generateRandomName(11).toStdString() + "." + m_domainName;

        setPeerName(m_domainName);
    }

protected:
    void givenMultipleConnectionsFromPeersOfTheSameDomain()
    {
        for (const auto& peerName: m_peerNames)
            addConnection(peerName);
    }

    void assertConnectionCountPerDomainIncludesAllPeers()
    {
        ASSERT_EQ(m_peerNames.size(), pool().getConnectionCountByPeerName(m_domainName));
    }

    void assertPeerIsFoundByDomainNamePrefix()
    {
        const auto foundPeerName = pool().findListeningPeerByDomain(m_domainName);
        ASSERT_FALSE(foundPeerName.empty());
        ASSERT_NE(
            m_peerNames.end(),
            std::find(m_peerNames.begin(), m_peerNames.end(), foundPeerName));
    }

    void assertPeerIsFoundByFullName()
    {
        ASSERT_EQ(
            m_peerNames[0],
            pool().findListeningPeerByDomain(m_peerNames[0]));
    }

    const std::string& domainName() const
    {
        return m_domainName;
    }

private:
    std::string m_domainName;
    std::vector<std::string> m_peerNames;
};

TEST_F(ListeningPeerPoolFindPeerByParentDomainName, getConnectionCountByPeerName)
{
    givenMultipleConnectionsFromPeersOfTheSameDomain();
    assertConnectionCountPerDomainIncludesAllPeers();
}

TEST_F(ListeningPeerPoolFindPeerByParentDomainName, peer_with_idle_connections_is_online)
{
    givenMultipleConnectionsFromPeersOfTheSameDomain();
    ASSERT_TRUE(pool().isPeerOnline(domainName()));
}

TEST_F(ListeningPeerPoolFindPeerByParentDomainName, take_idle_connection)
{
    givenMultipleConnectionsFromPeersOfTheSameDomain();
    whenRequestedConnection();
    thenConnectionHasBeenProvided();
}

TEST_F(ListeningPeerPoolFindPeerByParentDomainName, find_peer_by_prefix)
{
    givenMultipleConnectionsFromPeersOfTheSameDomain();
    assertPeerIsFoundByDomainNamePrefix();
    assertPeerIsFoundByFullName();
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
