#include "p2p_message_bus_test_base.h"

#include <nx/vms/api/data/camera_data.h>

#include <core/resource_management/resource_pool.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx_ec/dummy_handler.h>

namespace nx {
namespace p2p {
namespace test {

static const int kIterations = 100;

class P2pSyncWhileAddNewDataTest: public P2pMessageBusTestBase
{
protected:

    void givenTwoConnectedServers()
    {
        m_servers.clear();
        ec2::Appserver2Process::resetInstanceCounter();

        const int instanceCount = 2;
        startServers(instanceCount);

        for (auto& server : m_servers)
            ASSERT_TRUE(server->moduleInstance()->createInitialData("P2pTestSystem"));

        fullConnect(m_servers);

        auto resourcePool = m_servers[0]->moduleInstance()->commonModule()->resourcePool();
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (resourcePool->getAllServers(Qn::ResourceStatus::Online).size() != 2);
    }

    void createTestDataAsync()
    {
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
        for (int i = 0; i < kIterations; ++i)
        {
            const auto& moduleGuid = m_servers[0]->moduleInstance()->commonModule()->moduleGUID();

            nx::vms::api::UserData userData;
            userData.id = QnUuid::createUuid();
            userData.name = lm("user_%1").arg(i);
            userData.isEnabled = true;

            std::vector<nx::vms::api::UserData> users;
            users.push_back(userData);

            int index0 = i % 2;
            auto connection = m_servers[index0]->moduleInstance()->commonModule()->ec2Connection();
            auto userManager = connection->makeUserManager(Qn::kSystemAccess);
            userManager->save(
                users, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
    }

    void waitForSync()
    {
        int syncDoneCounter = 0;
        do
        {
            syncDoneCounter = 0;
            for (const auto& server: m_servers)
            {
                const auto& resPool = server->moduleInstance()->commonModule()->resourcePool();
                const auto& userList = resPool->getResources<QnUserResource>();
                if (userList.size() == kIterations + 1)
                    ++syncDoneCounter;
                else
                    break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (syncDoneCounter != m_servers.size());
    }
};

TEST_F(P2pSyncWhileAddNewDataTest, main)
{
    givenTwoConnectedServers();
    createTestDataAsync();
    waitForSync();
}

} // namespace test
} // namespace p2p
} // namespace nx
