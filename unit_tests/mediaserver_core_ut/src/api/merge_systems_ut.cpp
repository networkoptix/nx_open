#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <api/model/merge_system_data.h>
#include <api/model/getnonce_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

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

    void whenServerMergeRequestIssued(const LauncherPtr& requestTarget, const LauncherPtr& serverToMerge)
    {
        QnGetNonceReply getNonceData;
        issueGetRequest(requestTarget.get(), "api/getNonce", getNonceData);
        ASSERT_FALSE(getNonceData.nonce.isEmpty());
        ASSERT_FALSE(getNonceData.realm.isEmpty());

        MergeSystemData mergeSystemData;

        mergeSystemData.url = requestTarget->apiUrl().toString();
        //mergeSystemData.postKey =
        //     QString::fromLatin1(createHttpQueryAuthParam(
        //        ctx.auth.user(),
        //        ctx.auth.password(),
        //        nonceReply.realm,
        //        "POST",
        //        nonceReply.nonce.toUtf8()));
    }

    LauncherPtr givenServerLaunched(int port, SafeMode mode)
    {
        LauncherPtr result = std::unique_ptr<MediaServerLauncher>(
            new MediaServerLauncher(/* tmpDir */ "", port));

        result->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        if (mode == SafeMode::on)
            result->addSetting("ecDbReadOnly", "true");

        [&]() { ASSERT_TRUE(result->start()); }();

        vms::api::ModuleInformation moduleInformation;
        issueGetRequest( result.get(), "api/moduleInformation", moduleInformation);
        [&]() { ASSERT_FALSE(moduleInformation.id.isNull()); }();

        return result;
    }

    void thenResultCodeShouldBe(network::http::StatusCode::Value desiredCode)
    {
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

TEST_F(MergeSystems, DISABLED_SafeMode)
{
    auto server1 = givenServerLaunched(8901, SafeMode::on);
    auto server2 = givenServerLaunched(8902, SafeMode::off);
    whenServerMergeRequestIssued(/* requestTarget */ server2, /* serverToMerge */ server1);
    thenResultCodeShouldBe(network::http::StatusCode::forbidden);
}

} // namespace test
} // namespace nx
