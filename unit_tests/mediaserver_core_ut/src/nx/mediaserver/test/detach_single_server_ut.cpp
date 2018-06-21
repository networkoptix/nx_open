#include <gtest/gtest.h>
#include <server/server_globals.h>
#include <test_support/utils.h>
#include <test_support/mediaserver_launcher.h>
#include <api/app_server_connection.h>
#include <nx/network/http/http_client.h>

#include <nx/fusion/model_functions.h>
#include "common/common_module.h"
#include <api/global_settings.h>
#include <rest/handlers/detach_from_cloud_rest_handler.h>
#include <api/model/detach_from_cloud_data.h>

#include <test_support/appserver2_process.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <media_server/server_connector.h>
#include <api/test_api_requests.h>
#include <nx/network/app_info.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace mediaserver {
namespace test {

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2Process>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;
using namespace nx::test;

static const std::chrono::seconds kSyncWaitTimeout(10 * 1000);

class DetachSingleServerTest: public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        mediaServerLauncher.reset(new MediaServerLauncher());
        server2.reset(new Appserver2());

        const auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath() +
            lit("/detach_test.data");
        QDir(tmpDir).removeRecursively();
        server2->addArg(lit("--dbFile=%1").arg(tmpDir).toStdString().c_str());

        ASSERT_TRUE(mediaServerLauncher->start());
        ASSERT_TRUE(server2->startAndWaitUntilStarted());

        mediaServerLauncher->commonModule()->ec2Connection()->startReceivingNotifications();
    }

    static void TearDownTestCase()
    {
        mediaServerLauncher->commonModule()->ec2Connection()->stopReceivingNotifications();

        server2.reset();
        mediaServerLauncher.reset();
    }

    void givenCloudUser()
    {
        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);

        vms::api::UserData userData;
        userData.id = QnUuid::createUuid();
        userData.name = "Vasja pupkin@gmail.com";
        userData.email = userData.name;
        userData.isEnabled = true;
        userData.isCloud = true;
        userData.realm = nx::network::AppInfo::realm();
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(userData));

        auto settings = mediaServerLauncher->commonModule()->globalSettings();
        settings->setCloudSystemId(QnUuid::createUuid().toSimpleString());
        settings->setCloudAuthKey(QnUuid::createUuid().toSimpleString());
        settings->synchronizeNowSync();
    }

    nx::utils::Url makeUrl(const Appserver2Ptr& appserver2, const QString& path) const
    {
        auto endpoint = server2->moduleInstance()->endpoint();
        return lit("http://%1:%2%3")
            .arg(endpoint.address.toString())
            .arg(endpoint.port)
            .arg(path);
    }

    void givenTwoSynchronizedServers()
    {
        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();
        vms::api::UserDataList users;
        ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
        ASSERT_EQ(2, users.size());

        nx::vms::discovery::ModuleEndpoint endpoint2;
        endpoint2.id = server2->moduleInstance()->commonModule()->moduleGUID();
        endpoint2.endpoint = server2->moduleInstance()->endpoint();
        QnServerConnector::instance()->addConnection(endpoint2);

        vms::api::UserDataList users2;
        nx::utils::ElapsedTimer timer;
        timer.restart();
        while (users2.size() != 2 && timer.elapsed() < kSyncWaitTimeout)
            NX_TEST_API_GET(makeUrl(server2, "/ec2/getUsers"), &users2);
        ASSERT_EQ(2, users2.size());
    }

    void whenDetachFirstServerFromSystem()
    {
        auto client = std::make_unique<nx::network::http::HttpClient>();
        client->setUserName("admin");
        client->setUserPassword("admin");
        DetachFromCloudData data;
        data.password = "admin";

        nx::utils::Url url = mediaServerLauncher->apiUrl();
        url.setPath("/api/detachFromSystem");
        client->doPost(url, "application/json", QJson::serialized(data));
    }

    void thenUserRemovedFromFirstServerOnly()
    {
        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();
        vms::api::UserDataList users;
        ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
        ASSERT_EQ(1, users.size());

        // Make sure server2 is not synchronized with server1 any more
        for (int i = 0; i < 10; ++i)
        {
            vms::api::UserDataList users2;
            NX_TEST_API_GET(makeUrl(server2, "/ec2/getUsers"), &users2);
            ASSERT_EQ(2, users2.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    static std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
    static Appserver2Ptr server2;
};
std::unique_ptr<MediaServerLauncher> DetachSingleServerTest::mediaServerLauncher;
Appserver2Ptr  DetachSingleServerTest::server2;

TEST_F(DetachSingleServerTest, main)
{
    givenCloudUser();
    givenTwoSynchronizedServers();

    whenDetachFirstServerFromSystem();
    thenUserRemovedFromFirstServerOnly();
}

} // namespace test
} // namespace mediaserver
} // namespace nx
