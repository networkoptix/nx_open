#include "p2p_message_bus_base_ut.h"

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
#include <config.h>
#include <nx/p2p/p2p_serialization.h>
#include <api/common_message_processor.h>

namespace nx {
namespace p2p {
namespace test {

class MessageBusUnicast: public P2pMessageBusTestBase
{
public:
    ~MessageBusUnicast()
    {
        if (!m_servers.empty())
        {
            auto bus = dynamic_cast<MessageBus*>(m_servers[0]->moduleInstance()->ecConnection()->messageBus());
            QObject::disconnect(bus, nullptr, nullptr, nullptr);
        }
    }
protected:
    static const int kWaitTimeout = 1000 * 10;

    bool isAllServersOnlineCond()
    {
        return m_alivePeers.size() == 3;
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
        for (const auto& server: m_servers)
        {
            createData(server, 0, 0);
            MessageBus* bus = dynamic_cast<MessageBus*>(server->moduleInstance()->ecConnection()->messageBus());
            auto intervals = bus->delayIntervals();
            intervals.sendPeersInfoInterval = std::chrono::milliseconds(1);
            intervals.outConnectionsInterval = std::chrono::milliseconds(1);
            bus->setDelayIntervals(intervals);
            QObject::connect(
                server->moduleInstance()->commonModule()->messageProcessor(),
                &QnCommonMessageProcessor::businessActionReceived,
                [this](const QnAbstractBusinessActionPtr& action)
                {
                    ++m_actionReceived;
                });
        }

        MessageBus* bus = dynamic_cast<MessageBus*>(m_servers[0]->moduleInstance()->ecConnection()->messageBus());
        QObject::connect(
            bus,
            &ec2::QnTransactionMessageBusBase::peerFound,
            [this](const ApiPeerData& peer)
        {
            auto result = m_alivePeers.insert(peer);
        }
        );

        serverConnectFunc(m_servers);
    }

    void testMain()
    {
        startAllServers(sequenceConnect);
        waitForCondition(std::bind(&MessageBusUnicast::isAllServersOnlineCond, this));

        MessageBus* bus = dynamic_cast<MessageBus*>(m_servers[0]->moduleInstance()->ecConnection()->messageBus());
        QnTransaction<ApiBusinessActionData> transaction(
            m_servers[0]->moduleInstance()->commonModule()->moduleGUID());
        transaction.command = ec2::ApiCommand::broadcastAction;
        QnPeerSet dstPeers;
        dstPeers << m_servers[2]->moduleInstance()->commonModule()->moduleGUID();
        dstPeers << m_servers[3]->moduleInstance()->commonModule()->moduleGUID();
        bus->sendTransaction(transaction, dstPeers);
        waitForCondition(
            [this]()
            {
                return m_actionReceived == 2;
            });
    }

private:
    std::set<ApiPersistentIdData> m_alivePeers;
    std::atomic<int> m_actionReceived{};
};

TEST_F(MessageBusUnicast, main)
{
    testMain();
}

} // namespace test
} // namespace p2p
} // namespace nx
