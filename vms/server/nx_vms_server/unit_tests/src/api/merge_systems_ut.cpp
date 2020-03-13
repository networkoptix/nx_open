#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <api/model/merge_system_data.h>
#include <api/model/configure_system_data.h>
#include <api/model/getnonce_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/server/json_rest_result.h>
#include <network/authutil.cpp>

#include "test_api_requests.h"
#include <utils/merge_systems_common.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/full_info_data.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/merge_status_reply.h>

using MergeStatus = utils::MergeSystemsStatus::Value;
using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

namespace nx {
namespace test {

enum class MergeAuthType
{
    rawPassword,
    getPostKeys,
};

class MergeSystems: public ::testing::TestWithParam<MergeAuthType>
{
protected:
    enum class SafeMode
    {
        on,
        off
    };

    QnJsonRestResult merge(
        const LauncherPtr& target,
        const QString& password,
        const MergeSystemData& data)
    {
        QByteArray responseBody;
        [&]
        {
            NX_TEST_API_POST(
                target.get(), "api/mergeSystems", data,
                [](const QByteArray& data) { return data; },
                Equals(nx::network::http::StatusCode::ok),
                "admin", password, &responseBody);
        }();

        bool success = false;
        const auto result = QJson::deserialized<QnJsonRestResult>(responseBody, {}, &success);
        EXPECT_TRUE(success);
        return result;
    }

    void addAuth(MergeSystemData* data, const LauncherPtr& target, const QString& password)
    {
        switch (GetParam())
        {
            case MergeAuthType::rawPassword:
                {
                    nx::utils::Url url(data->url);
                    url.setUserName("admin");
                    url.setPassword(password);
                    data->url = url.toString();
                }
                break;

            case MergeAuthType::getPostKeys:
                {
                    QnGetNonceReply nonceReply;
                    issueGetRequest(target.get(), "api/getNonce", nonceReply);
                    ASSERT_FALSE(nonceReply.nonce.isEmpty());
                    ASSERT_FALSE(nonceReply.realm.isEmpty());
                    data->postKey = QString::fromLatin1(createHttpQueryAuthParam(
                        "admin", password, nonceReply.realm,
                        "POST", nonceReply.nonce.toUtf8()));
                    data->getKey = QString::fromLatin1(createHttpQueryAuthParam(
                        "admin", password, nonceReply.realm,
                        "GET", nonceReply.nonce.toUtf8()));
                }
                break;
        };
    }

    void assertMergeRequestReturn(
        const LauncherPtr& requestTarget,
        const LauncherPtr& serverToMerge,
        MergeStatus mergeStatus,
        nx::vms::api::SystemMergeHistoryRecord* outMergeResult = nullptr,
        const QString& requestTargetPassword = "admin",
        const QString& serverToMergePassword = "admin",
        bool testDryRun = true)
    {
        MergeSystemData mergeSystemData;
        mergeSystemData.url = serverToMerge->apiUrl().toString();
        if (testDryRun)
        {
            mergeSystemData.dryRun = true;
            const auto testResult = merge(requestTarget, requestTargetPassword, mergeSystemData);
            EXPECT_EQ(toString(mergeStatus), testResult.errorString) << "on Dry Run";
        }

        mergeSystemData.dryRun = false;
        addAuth(&mergeSystemData, requestTarget, serverToMergePassword);
        const auto actualResult = merge(requestTarget, requestTargetPassword, mergeSystemData);
        ASSERT_EQ(toString(mergeStatus), actualResult.errorString) << "on Actual Merge";
        if (outMergeResult)
            *outMergeResult = actualResult.deserialized<nx::vms::api::SystemMergeHistoryRecord>();
    }

    LauncherPtr givenServer()
    {
        LauncherPtr result = std::unique_ptr<MediaServerLauncher>(
            new MediaServerLauncher(/* tmpDir */ ""));

        result->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        return result;
    }

    void whenServerLaunched(const LauncherPtr& server, SafeMode mode)
    {
        if (mode == SafeMode::on)
            server->addSetting("ecDbReadOnly", "true");

        ASSERT_TRUE(server->start());

        vms::api::ModuleInformation moduleInformation;
        issueGetRequest(server.get(), "api/moduleInformation", moduleInformation);
        ASSERT_FALSE(moduleInformation.id.isNull());
    }

    void whenServerStopped(const LauncherPtr& server)
    {
        server->stop();
    }

    void whenServerIsConfigured(const LauncherPtr& server)
    {
        ConfigureSystemData configureData;
        configureData.localSystemId = QnUuid::createUuid();
        configureData.systemName = configureData.localSystemId.toString();

        NX_TEST_API_POST(server.get(), "api/configure", configureData);
    }

    void whenAdminPasswordHasChanged(const LauncherPtr& server, const QString& password)
    {
        vms::api::UserDataEx userData;
        userData.password = password;
        userData.id = server->commonModule()->resourcePool()->getAdministrator()->getId();
        NX_TEST_API_POST(server.get(), "ec2/saveUser", userData,
            keepOnlyJsonFields({ "password", "id" }));

        std::vector<vms::api::UserData> resultData;
        NX_TEST_API_GET(server.get(), "/ec2/getUsers", &resultData,
            nx::network::http::StatusCode::ok, "admin", password);
        NX_ASSERT(1, resultData.size());
    }

    void whenServerHasData(const LauncherPtr& server, const QString& adminPassword)
    {
        const auto serverGuid = server->commonModule()->moduleGUID().toString();

        nx::vms::api::CameraData cameraData;
        cameraData.parentId = QnUuid::createUuid();
        cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
        cameraData.vendor = "test vendor";
        cameraData.physicalId =
            lit("matching physicalId %1").arg(serverGuid);
        cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
        NX_TEST_API_POST(server.get(), "/ec2/saveCamera", cameraData,
            nullptr, Equals(nx::network::http::StatusCode::ok), "admin", adminPassword);

        vms::api::UserDataList users;
        vms::api::UserData userData;
        userData.id = QnUuid::createUuid();
        userData.fullName = lit("user %1").arg(serverGuid);
        userData.permissions = GlobalPermission::admin;
        NX_TEST_API_POST(server.get(), "/ec2/saveUser", userData,
            nullptr, Equals(nx::network::http::StatusCode::ok), "admin", adminPassword);

        nx::vms::api::LayoutData layoutData;
        layoutData.name = "fixed layout name";
        layoutData.id = QnUuid::createUuid();
        layoutData.parentId = userData.id;
        nx::vms::api::LayoutItemData item;
        item.resourceId = cameraData.id;
        item.id = QnUuid::createUuid();
        layoutData.items.push_back(item);
        NX_TEST_API_POST(server.get(), "/ec2/saveLayout", layoutData,
            nullptr, Equals(nx::network::http::StatusCode::ok), "admin", adminPassword);
    }

    void thenFullInfoEqual(const std::vector<LauncherPtr>& servers,
        const QString& adminPassword = "admin")
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));

        vms::api::FullInfoData fullInfoData;
        NX_TEST_API_GET(servers[0].get(), "/ec2/getFullInfo", &fullInfoData,
            nx::network::http::StatusCode::ok, "admin", adminPassword);

        ASSERT_EQ(servers.size(), fullInfoData.servers.size());
        ASSERT_EQ(servers.size(), fullInfoData.cameras.size());
        ASSERT_EQ(servers.size() + 1, fullInfoData.users.size());
        ASSERT_EQ(servers.size(), fullInfoData.layouts.size());

        for (int i = 1; i < servers.size(); ++i)
        {
            vms::api::FullInfoData fullInfoData2;
            NX_TEST_API_GET(servers[0].get(), "/ec2/getFullInfo", &fullInfoData2,
                nx::network::http::StatusCode::ok, "admin", adminPassword);
            ASSERT_EQ(QJson::serialized(fullInfoData), QJson::serialized(fullInfoData2));
        }
    }

private:
    template<typename ResponseData>
    void issueGetRequest(
        MediaServerLauncher* launcher,
        const QString& path,
        ResponseData& responseData)
    {
        QnJsonRestResult jsonRestResult;
        NX_TEST_API_GET(launcher, path, &jsonRestResult);
        responseData = jsonRestResult.deserialized<ResponseData>();
    }
};

TEST_P(MergeSystems, SafeMode_From)
{
    auto server1 = givenServer();
    whenServerLaunched(server1, SafeMode::off);
    whenServerIsConfigured(server1);
    whenServerStopped(server1);
    whenServerLaunched(server1, SafeMode::on);

    auto server2 = givenServer();
    whenServerLaunched(server2, SafeMode::off);
    whenServerIsConfigured(server2);

    assertMergeRequestReturn(
        /* requestTarget */ server1,
        /* serverToMerge */ server2,
        /* expectedCode */ MergeStatus::safeMode);
}

TEST_P(MergeSystems, SafeMode_To)
{
    auto server1 = givenServer();
    whenServerLaunched(server1, SafeMode::off);
    whenServerIsConfigured(server1);

    auto server2 = givenServer();
    whenServerLaunched(server2, SafeMode::off);
    whenServerIsConfigured(server2);
    whenServerStopped(server2);
    whenServerLaunched(server2, SafeMode::on);

    assertMergeRequestReturn(
        /* requestTarget */ server1,
        /* serverToMerge */ server2,
        /* expectedCode */ MergeStatus::safeMode,
        /* outMergeResult */ nullptr,
        /* requestTargetPassword */ "admin",
        /* serverToMergePassword */ "admin",
        /* testDryRun */ false); //< Dry run is not supported if target server is in the test mode.
}

void waitForMergeFinished(
    const LauncherPtr& server,
    const QString& adminPassword = "admin")
{
    int success = 0;
    do
    {
        QnJsonRestResult jsonResult;
        NX_TEST_API_GET(server.get(), "/ec2/mergeStatus", &jsonResult,
            nx::network::http::StatusCode::ok, "admin", adminPassword);
        auto mergeStatus = jsonResult.deserialized<vms::api::MergeStatusReply>();
        if (!mergeStatus.mergeId.isNull() && !mergeStatus.mergeInProgress)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (true);
}

TEST_P(MergeSystems, DoubleMergeWithTakeLocalSettings)
{
    std::vector<LauncherPtr> servers;
    nx::vms::api::SystemMergeHistoryRecord mergeResult;
    for (int i = 0; i < 3; ++i)
    {
        auto server = givenServer();
        whenServerLaunched(server, SafeMode::off);
        whenServerIsConfigured(server);
        servers.push_back(std::move(server));
    }

    assertMergeRequestReturn(
        /* requestTarget */ servers[0],
        /* serverToMerge */ servers[1],
        /* expectedCode */ MergeStatus::ok,
        &mergeResult);

    waitForMergeFinished(servers[0]);

    assertMergeRequestReturn(
        /* requestTarget */ servers[2],
        /* serverToMerge */ servers[1],
        /* expectedCode */ MergeStatus::ok,
        &mergeResult);

    waitForMergeFinished(servers[0]);
}

TEST_P(MergeSystems, MergeServersWithData)
{
    const int kServerCount = 3;
    std::vector<LauncherPtr> servers;
    nx::vms::api::SystemMergeHistoryRecord mergeResult;
    for (int i = 0; i < kServerCount; ++i)
    {
        auto server = givenServer();
        whenServerLaunched(server, SafeMode::off);
        whenServerHasData(server, /*password*/ "admin");
        whenServerIsConfigured(server);
        servers.push_back(std::move(server));
    }

    for (int i = 0; i < kServerCount - 1; ++i)
    {
        assertMergeRequestReturn(
            /* requestTarget */ servers[i],
            /* serverToMerge */ servers[i+1],
            /* expectedCode */ MergeStatus::ok,
            &mergeResult);
    }
    waitForMergeFinished(servers[0]);
    thenFullInfoEqual(servers);
}

TEST_P(MergeSystems, MergeServersWithDifferentPasswords)
{
    const int kServerCount = 2;
    std::vector<LauncherPtr> servers;
    nx::vms::api::SystemMergeHistoryRecord mergeResult;
    for (int i = 0; i < kServerCount; ++i)
    {
        auto server = givenServer();
        const auto password = lit("admin_%1").arg(i);
        whenServerLaunched(server, SafeMode::off);
        whenServerIsConfigured(server);
        whenAdminPasswordHasChanged(server, password);
        whenServerHasData(server, password);
        servers.push_back(std::move(server));
    }

    for (int i = 0; i < kServerCount - 1; ++i)
    {
        assertMergeRequestReturn(
            /* requestTarget */ servers[i],
            /* serverToMerge */ servers[i + 1],
            /* expectedCode */ MergeStatus::ok,
            &mergeResult,
            lit("admin_%1").arg(i),
            lit("admin_%1").arg(i+1));
    }
    waitForMergeFinished(servers[0], "admin_0");
    thenFullInfoEqual(servers, "admin_0");
}

INSTANTIATE_TEST_CASE_P(Api, MergeSystems, ::testing::Values(
    MergeAuthType::rawPassword,
    MergeAuthType::getPostKeys
));

} // namespace test
} // namespace nx
