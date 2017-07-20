#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>

#include <nx/p2p/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/p2p_connection.h>
#include <ini.h>
#include <nx/p2p/p2p_serialization.h>

namespace nx {
namespace p2p {
namespace test {

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
    static const int kWaitTimeout = 1000 * 10;

    bool isAllServersOnlineCond()
    {
        return m_alivePeers.size() == 3;
    }

    bool isPeerLost(int index)
    {
        const auto connection = m_servers[index]->moduleInstance()->ecConnection();
        auto bus = connection->messageBus()->dynamicCast<MessageBus*>();
        ApiPersistentIdData lostPeer = bus->localPeer();
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

    void startAllServers(std::function<void(std::vector<Appserver2Ptr>&)> serverConnectFunc)
    {
        const_cast<bool&>(ec2::ini().isP2pMode) = true;
        startServers(4);
        for (const auto& server : m_servers)
        {
            createData(server, 0, 0);
            const auto connection = server->moduleInstance()->ecConnection();
            MessageBus* bus = connection->messageBus()->dynamicCast<MessageBus*>();
            auto intervals = bus->delayIntervals();
            intervals.sendPeersInfoInterval = std::chrono::milliseconds(1);
            intervals.outConnectionsInterval = std::chrono::milliseconds(1);
            bus->setDelayIntervals(intervals);
        }

        const auto connection = m_servers[0]->moduleInstance()->ecConnection();
        MessageBus* bus = connection->messageBus()->dynamicCast<MessageBus*>();
        QObject::connect(
            bus,
            &ec2::TransactionMessageBusBase::peerFound,
            [this](QnUuid peer, Qn::PeerType)
            {
                auto result = m_alivePeers.insert(peer);
                ASSERT_TRUE(result.second);
            }
        );
        QObject::connect(
            bus,
            &ec2::TransactionMessageBusBase::peerLost,
            [this](QnUuid peer, Qn::PeerType)
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
        startAllServers(circleConnect);
        ASSERT_TRUE(m_servers.size() >= 3);
        m_servers[1]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        m_servers[2]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 2));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerAlive, this, 3));
    }

    void sequenceConnectMain()
    {
        startAllServers(sequenceConnect);
        ASSERT_TRUE(m_servers.size() >= 2);
        m_servers[1]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 2));
        waitForCondition(std::bind(&MessageBusOnlinePeers::isPeerLost, this, 3));
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

} // namespace test
} // namespace p2p
} // namespace nx
