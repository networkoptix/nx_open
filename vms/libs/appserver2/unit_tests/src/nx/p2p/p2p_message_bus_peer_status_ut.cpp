#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx/vms/api/data/camera_data.h>
#include <core/resource_access/user_access_data.h>

#include <nx/p2p/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/p2p_connection.h>
#include <nx/p2p/p2p_serialization.h>

namespace nx {
namespace p2p {
namespace test {

static const int kServerCount = 6;

class MessageBusOnlinePeers: public P2pMessageBusTestBase
{
public:
    ~MessageBusOnlinePeers()
    {
        if (!m_servers.empty())
        {
            const auto connection = m_servers[0]->moduleInstance()->ecConnection();
            auto bus = connection->messageBus()->dynamicCast<MessageBus*>();
            QObject::disconnect(bus, nullptr, nullptr, nullptr);
        }
    }

protected:
    static const int kWaitTimeout = 1000 * 15 * 2;

    bool isAllServersOnlineCond()
    {
        if (m_alivePeers.size() != kServerCount -1)
            return false;

        for (int i = 0; i < m_servers.size(); ++i)
        {
            const auto& server = m_servers[i];
            const auto pool = server->moduleInstance()->commonModule()->resourcePool();
            int serverCount = pool->getAllServers(Qn::AnyStatus).size();
            if (serverCount != kServerCount)
                return false;
        }

        return true;
    }

    qint32 serverDistance(int from, int to)
    {
        const auto& serverFrom = m_servers[from];
        const auto connection = serverFrom->moduleInstance()->ecConnection();
        auto bus = connection->messageBus()->dynamicCast<MessageBus*>();
        const auto toId = m_servers[to]->moduleInstance()->commonModule()->moduleGUID();
        int distance = bus->distanceToPeer(toId);
        return distance;
    }

    bool isPeerLost(int index)
    {
        const auto connection = m_servers[index]->moduleInstance()->ecConnection();
        auto bus = connection->messageBus()->dynamicCast<MessageBus*>();
        vms::api::PersistentIdData lostPeer = bus->localPeer();
        return m_alivePeers.find(lostPeer.id) == m_alivePeers.end();
    }
    bool isPeerAlive(int index)
    {
        return !isPeerLost(index);
    }

    void waitForCondition(std::function<bool ()> condition)
    {
        QElapsedTimer timer;
        timer.restart();
        while (!condition() && timer.elapsed() < kWaitTimeout)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ASSERT_TRUE(condition());
    }

    void startAllServers(std::function<void(std::vector<Appserver2Ptr>&)> serverConnectFunc, int serverCount)
    {
        startServers(serverCount);
        for (const auto& server : m_servers)
        {
            createData(server, 0, 0);
        }
        setLowDelayIntervals();

        const auto connection = m_servers[0]->moduleInstance()->ecConnection();
        MessageBus* bus = connection->messageBus()->dynamicCast<MessageBus*>();
        QObject::connect(
            bus,
            &ec2::TransactionMessageBusBase::peerFound,
            [this](QnUuid peer, nx::vms::api::PeerType)
            {
                auto result = m_alivePeers.insert(peer);
                ASSERT_TRUE(result.second);
            }
        );
        QObject::connect(
            bus,
            &ec2::TransactionMessageBusBase::peerLost,
            [this](QnUuid peer, nx::vms::api::PeerType)
            {
                auto oldSize = m_alivePeers.size();
                m_alivePeers.erase(peer);
                auto newSize = m_alivePeers.size();
                ASSERT_TRUE(oldSize > newSize);
            }
        );

        serverConnectFunc(m_servers);
        waitForCondition(std::bind(&MessageBusOnlinePeers::isAllServersOnlineCond, this));
    }

    void rhombConnectMain()
    {
        startAllServers(circleConnect, kServerCount);
        ASSERT_TRUE(m_servers.size() >= 3);
        m_servers[1]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        m_servers[2]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 2));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerAlive, this, 3));
    }

    void sequenceConnectMain()
    {
        startAllServers(sequenceConnect, kServerCount);
        ASSERT_TRUE(m_servers.size() == kServerCount);
        m_servers[1]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 2));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 3));

		// It should have some offline distance because server[0] has got some data.
        for (int i = 0; i < kServerCount; ++i)
            ASSERT_LT(serverDistance(0, i), kMaxDistance);
    }

    void fullConnectMain()
    {
        startAllServers(fullConnect, kServerCount);
        ASSERT_TRUE(m_servers.size() == kServerCount);

        waitForSync(0);

        const int dropIndex = 1;
        const QnUuid id = m_servers[dropIndex]->moduleInstance()->commonModule()->moduleGUID();
        m_servers[dropIndex]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        for (int i = 0; i < kServerCount; ++i)
            m_servers[i]->moduleInstance()->ecConnection()->messageBus()->removeOutgoingConnectionFromPeer(id);

        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, dropIndex));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, dropIndex));

        // It should have some offline distance because server[0] has got some data.
        for (int i = 0; i < kServerCount; ++i)
            ASSERT_LT(serverDistance(0, i), kMaxDistance);
    }

    void extendedCircleConnectMain()
    {
        auto connectServers = [](std::vector<Appserver2Ptr>& servers)
        {
            bidirectConnectServers(servers[0], servers[1]);
            bidirectConnectServers(servers[1], servers[2]);
            bidirectConnectServers(servers[2], servers[3]);
            bidirectConnectServers(servers[3], servers[0]);

            bidirectConnectServers(servers[0], servers[4]);
            bidirectConnectServers(servers[4], servers[5]);
        };

        startAllServers(connectServers, 6);
        ASSERT_TRUE(m_servers.size() == 6);

        waitForSync(0);

        const int dropIndex = 1;
        const QnUuid id = m_servers[dropIndex]->moduleInstance()->commonModule()->moduleGUID();
        m_servers[dropIndex]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        for (int i = 0; i < kServerCount; ++i)
            m_servers[i]->moduleInstance()->ecConnection()->messageBus()->removeOutgoingConnectionFromPeer(id);

        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, dropIndex));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 0));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, dropIndex));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 0));

        // It should have some offline distance because server[0] has got some data.
        for (int i = 0; i < kServerCount; ++i)
            ASSERT_LT(serverDistance(0, i), kMaxDistance);
    }

private:
    std::set<QnUuid> m_alivePeers;
};

TEST_F(MessageBusOnlinePeers, rhombConnect)
{
    rhombConnectMain();
}

TEST_F(MessageBusOnlinePeers, sequenceConnect)
{
    sequenceConnectMain();
}

TEST_F(MessageBusOnlinePeers, fullConnect)
{
    fullConnectMain();
}

TEST_F(MessageBusOnlinePeers, extendedCircleConnect)
{
    extendedCircleConnectMain();
}

} // namespace test
} // namespace p2p
} // namespace nx
