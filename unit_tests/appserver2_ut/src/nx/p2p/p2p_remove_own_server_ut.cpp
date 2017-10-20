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
#include <nx/p2p/p2p_serialization.h>
#include <nx_ec/dummy_handler.h>
#include <nx/utils/argument_parser.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace p2p {
namespace test {

static const std::chrono::seconds kMaxSyncTimeout(15 * 1000);

class P2pSpecialTransactionTest : public P2pMessageBusTestBase
{
public:

    void givenTwoServers()
    {
        startServers(2);
        for (const auto& server : m_servers)
            createData(server, 0, 0);
    }

    void givenTwoSynchronizedServers()
    {
        givenTwoServers();
        sequenceConnect(m_servers);

        QnUuid server1Id = m_servers[1]->moduleInstance()->commonModule()->moduleGUID();
        const auto connection0 = m_servers[0]->moduleInstance()->ecConnection();
        auto bus0 = connection0->messageBus()->dynamicCast<MessageBus*>();
        const auto& resPool0 = m_servers[0]->moduleInstance()->commonModule()->resourcePool();

        bool result = waitForCondition(
            [&]()
            {
                auto servers = resPool0->getAllServers(Qn::AnyStatus);
                auto distance = bus0->distanceToPeer(server1Id);
                return servers.size() == m_servers.size() && distance == 1;
            }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }

    void whenDisconnectServers()
    {
        m_servers[0]->moduleInstance()->ecConnection()->messageBus()->dropConnections();
        waitForCondition(
            [&]()
        {
            QnUuid server1Id = m_servers[1]->moduleInstance()->commonModule()->moduleGUID();
            const auto connection0 = m_servers[0]->moduleInstance()->ecConnection();
            auto bus0 = connection0->messageBus()->dynamicCast<MessageBus*>();
            auto distance = bus0->distanceToPeer(server1Id);
            return distance > 1;
        }, kMaxSyncTimeout);
    }

    void whenOneServerForgetsAnother()
    {
        auto connection = m_servers[0]->moduleInstance()->ecConnection();
        auto manager = connection->getMediaServerManager(Qn::kSystemAccess);
        manager->removeSync(m_servers[1]->moduleInstance()->commonModule()->moduleGUID());
    }

    void whenConnectServers()
    {
        sequenceConnect(m_servers);
    }

    void thenServersAreSynchronizedWithEachOther()
    {
        const auto& resPool0 = m_servers[0]->moduleInstance()->commonModule()->resourcePool();
        QnUuid server1Id = m_servers[1]->moduleInstance()->commonModule()->moduleGUID();
        bool result = waitForConditionOnAllServers(
            [&](const Appserver2Ptr& server)
        {
            const auto connection0 = m_servers[0]->moduleInstance()->ecConnection();
            auto bus0 = connection0->messageBus()->dynamicCast<MessageBus*>();
            auto servers = resPool0->getAllServers(Qn::AnyStatus);
            auto distance = bus0->distanceToPeer(server1Id);
            return servers.size() == m_servers.size() && distance == 1;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }
};


TEST_F(P2pSpecialTransactionTest, two_servers_still_abel_to_reconnect_when_one_server_forgets_another_while_offline)
{
    givenTwoSynchronizedServers();

    whenDisconnectServers();
    whenOneServerForgetsAnother();
    whenConnectServers();

    thenServersAreSynchronizedWithEachOther();
}

} // namespace test
} // namespace p2p
} // namespace nx
