#include <gtest/gtest.h>

#include <api/test_api_requests.h>
#include <common/common_module.h>
#include <media_server/server_connector.h>
#include <nx/utils/elapsed_timer.h>
#include <test_support/appserver2_process.h>
#include <test_support/mediaserver_launcher.h>
#include <api/test_api_requests.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <api/global_settings.h>

namespace nx {
namespace vms::server {
namespace test {

using namespace nx::test;

static const std::chrono::seconds kSyncWaitTimeout(30);

class AuthByAuthKeyTest: public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        const QnUuid systemId = QnUuid::createUuid();
        for (auto& server: servers)
        {
            server.reset(new MediaServerLauncher());
            ASSERT_TRUE(server->start());
            server->commonModule()->globalSettings()->setLocalSystemId(systemId);
            server->commonModule()->globalSettings()->synchronizeNowSync();
        }
    }

    static void TearDownTestCase()
    {
        for (auto& server: servers)
            server.reset();
    }

    void givenTwoSynchronizedServers()
    {
        servers[0]->connectTo(servers[1].get());

        auto ec2Connection = servers[0]->commonModule()->ec2Connection();
        auto ec2Connection2 = servers[1]->commonModule()->ec2Connection();

        vms::api::UserDataList users, users2;
        vms::api::MediaServerDataList servers, servers2;

        nx::utils::ElapsedTimer timer;
        timer.restart();
        int step = 0;
        bool isSynchronized = false;
        do
        {
            if (step++ > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
            ec2Connection2->getUserManager(Qn::kSystemAccess)->getUsersSync(&users2);
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&servers);
            ec2Connection2->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&servers2);
            isSynchronized = users == users2 && servers.size() == 2 && servers2.size() == 2;
        } while (!isSynchronized && timer.elapsed() < kSyncWaitTimeout);

        ASSERT_TRUE(isSynchronized);
    }

    void whenAdminPasswordChangedOnSecondServer()
    {
        auto commonModule = servers[1]->commonModule();
        auto adminUser = commonModule->resourcePool()->getAdministrator();

        vms::api::UserDataEx userData;
        ec2::fromResourceToApi(adminUser, userData);
        userData. password = "another password";

        NX_TEST_API_POST(servers[1].get(), "/ec2/saveUser", userData);
    }

    void whenStopFirstServer()
    {
        servers[0]->stop();
    }

    void whenStartFirstServer()
    {
        servers[0]->start();
    }

    static std::unique_ptr<MediaServerLauncher> servers[2];
};
std::unique_ptr<MediaServerLauncher> AuthByAuthKeyTest::servers[2];

TEST_F(AuthByAuthKeyTest, main)
{
    givenTwoSynchronizedServers();

    whenStopFirstServer();
    whenAdminPasswordChangedOnSecondServer();
    whenStartFirstServer();

    givenTwoSynchronizedServers();
}

} // namespace test
} // namespace vms::server
} // namespace nx
