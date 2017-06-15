#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/cloud/cdb/client/data/auth_data.h>

#include "mediaserver_cloud_integration_test_setup.h"

namespace {

static bool resourceParamPresent(
    const ec2::ApiResourceParamDataList& params,
    const QString& name)
{
    for (const auto& param: params)
    {
        if (param.name == name)
            return true;
    }

    return false;
}

} // namespace

class CloudUserOfflineLogin:
    public MediaServerCloudIntegrationTest,
    public ::testing::Test
{
protected:
    void givenCloudSystem()
    {
        waitUserCloudAuthInfoToBeSynchronized();
    }

    void whenSystemWentOffline()
    {
        cdb()->stop();
    }

    void whenMediaServerRestarted()
    {
        mediaServer().stop();
        ASSERT_TRUE(startMediaServer());
    }

    void thenUserCanStillLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        ec2::ApiResourceParamDataList vmsSettings;
        ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient.ec2GetSettings(&vmsSettings));
    }

private:
    virtual void SetUp() override
    {
        connectSystemToCloud();
    }

    void waitUserCloudAuthInfoToBeSynchronized()
    {
        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            ec2::ApiResourceParamDataList params;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                mediaServerClient.ec2GetResourceParams(
                    QnUuid::fromStringSafe(cloudOwnerVmsUserId()),
                    &params));
            if (resourceParamPresent(params, nx::cdb::api::kVmsUserAuthInfoAttributeName))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

TEST_F(CloudUserOfflineLogin, DISABLED_login_works_on_offline_server_after_restart)
{
    givenCloudSystem();
    whenSystemWentOffline();
    whenMediaServerRestarted();
    thenUserCanStillLogin();
}

// TEST_F(CloudUserOfflineLogin, multiple_users_can_login)

// TEST_F(CloudUserOfflineLogin, multiple_users_different_history_depth)

// TEST_F(CloudUserOfflineLogin, cloud_user_added_while_system_is_offline_cannot_login_while_others_can)

// TEST_F(CloudUserOfflineLogin, user_can_login_after_password_change)

// TEST_F(CloudUserOfflineLogin, user_password_changed_while_system_was_offline)
