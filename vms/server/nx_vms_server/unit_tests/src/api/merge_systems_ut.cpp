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

using MergeStatus = utils::MergeSystemsStatus::Value;

namespace nx {
namespace test {

class MergeSystems: public ::testing::Test
{
protected:
    enum class SafeMode
    {
        on,
        off
    };

    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    void assertMergeRequestReturn(
        const LauncherPtr& requestTarget,
        const LauncherPtr& serverToMerge,
        MergeStatus mergeStatus)
    {
        QnGetNonceReply nonceReply;
        issueGetRequest(requestTarget.get(), "api/getNonce", nonceReply);
        ASSERT_FALSE(nonceReply.nonce.isEmpty());
        ASSERT_FALSE(nonceReply.realm.isEmpty());

        MergeSystemData mergeSystemData;
        mergeSystemData.url = serverToMerge->apiUrl().toString();
        mergeSystemData.postKey = QString::fromLatin1(createHttpQueryAuthParam(
            "admin", "admin", nonceReply.realm, "POST", nonceReply.nonce.toUtf8()));
        mergeSystemData.getKey = QString::fromLatin1(createHttpQueryAuthParam(
            "admin", "admin", nonceReply.realm, "GET", nonceReply.nonce.toUtf8()));

        QByteArray responseBody;
        NX_TEST_API_POST(requestTarget.get(), "api/mergeSystems", mergeSystemData,
            [](const QByteArray& data) {return data;},
            nx::network::http::StatusCode::ok,
            "admin", "admin",
            &responseBody);
        bool success = false;
        auto result = QJson::deserialized<QnJsonRestResult>(responseBody, QnJsonRestResult(), &success);
        ASSERT_TRUE(success);
        ASSERT_EQ(toString(mergeStatus), result.errorString);
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

TEST_F(MergeSystems, SafeMode_From)
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

TEST_F(MergeSystems, SafeMode_To)
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
        /* expectedCode */ MergeStatus::safeMode);
}

} // namespace test
} // namespace nx
