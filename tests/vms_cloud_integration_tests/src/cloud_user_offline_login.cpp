#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/cloud/cdb/client/data/auth_data.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>

#include "mediaserver_cloud_integration_test_setup.h"
#include <nx/utils/string.h>

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
    public MediaServerCloudIntegrationTest
{
protected:
    void givenUserInvitedFromDesktopClient()
    {
        m_invitedUserEc2Data = inviteRandomCloudUser();
    }

    void whenCloudUserPasswordHasBeenChanged()
    {
        changeCloudOwnerAccountPassword();
    }

    void whenCloudDataHasBeenSynchronizedToTheServer()
    {
        waitForCloudDataSynchronizedToTheMediaServer();
    }

    void whenAddedMultipleCloudUsers()
    {
        constexpr int kAdditionalCloudUserCount = 2;
        for (int i = 0; i < kAdditionalCloudUserCount; ++i)
        {
            m_additionalCloudUsers.push_back(cdb()->addActivatedAccount2());
            cdb()->shareSystemEx(
                ownerAccount(),
                cloudSystem(),
                m_additionalCloudUsers.back().email,
                nx::cdb::api::SystemAccessRole::viewer);
        }
    }

    void whenSystemWentOffline()
    {
        waitForCloudDataSynchronizedToTheMediaServer();

        cdb()->stop();

        whenMediaServerRestarted();
    }

    void whenMediaServerRestarted()
    {
        mediaServer().stop();
        ASSERT_TRUE(startMediaServer());
    }

    void whenUserCompletesRegistrationInCloud()
    {
        m_invitedAccount.email = m_invitedUserEc2Data.email.toStdString();
        m_invitedAccount.password = nx::utils::generateRandomName(11).toStdString();

        waitForUserToAppearInCloud(m_invitedAccount.email);

        changeAccountPassword(m_invitedAccount.email, m_invitedAccount.password);

        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->getAccount(
                m_invitedAccount.email,
                m_invitedAccount.password,
                &m_invitedAccount));
    }

    void thenUserCanStillLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        ec2::ApiResourceParamDataList vmsSettings;
        ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient.ec2GetSettings(&vmsSettings));
    }

    void thenAllUsersCanStillLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        for (const auto& account: m_additionalCloudUsers)
        {
            mediaServerClient.setUserName(QString::fromStdString(account.email));
            mediaServerClient.setPassword(QString::fromStdString(account.password));
            ec2::ApiUserDataList users;
            ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient.ec2GetUsers(&users));
        }
    }

    void thenInvitedUserCanLoginToTheSystem()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        mediaServerClient.setUserName(QString::fromStdString(m_invitedAccount.email));
        mediaServerClient.setPassword(QString::fromStdString(m_invitedAccount.password));
        ec2::ApiUserDataList users;
        ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient.ec2GetUsers(&users));
    }

private:
    std::vector<nx::cdb::AccountWithPassword> m_additionalCloudUsers;
    ::ec2::ApiUserData m_invitedUserEc2Data;
    nx::cdb::AccountWithPassword m_invitedAccount;

    virtual void SetUp() override
    {
        connectSystemToCloud();
        waitUserCloudAuthInfoToBeSynchronized();
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

    void changeAccountPassword(const std::string& email, const std::string& password)
    {
        std::string confirmationCode;
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->resetAccountPassword(email, &confirmationCode));

        nx::cdb::api::AccountUpdateData accountUpdate;
        accountUpdate.passwordHa1 = nx_http::calcHa1(
            email.c_str(),
            nx::network::AppInfo::realm().toStdString().c_str(),
            password.c_str()).toStdString();

        // Confirmation code has format base64(tmp_password:email).
        const auto tmpPasswordAndEmail = QByteArray::fromBase64(
            QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
        const std::string tmpPassword =
            tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).toStdString();

        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->updateAccount(email, tmpPassword, accountUpdate));
    }
};

TEST_P(CloudUserOfflineLogin, login_works_on_offline_server_after_restart)
{
    whenSystemWentOffline();
    thenUserCanStillLogin();
}

TEST_P(CloudUserOfflineLogin, multiple_users_can_login)
{
    whenAddedMultipleCloudUsers();
    whenSystemWentOffline();

    thenAllUsersCanStillLogin();
}

// TEST_P(CloudUserOfflineLogin, multiple_users_different_history_depth)

// TEST_P(CloudUserOfflineLogin, cloud_user_added_while_system_is_offline_cannot_login_while_others_can)

TEST_P(CloudUserOfflineLogin, user_can_login_after_password_change)
{
    whenCloudUserPasswordHasBeenChanged();
    whenSystemWentOffline();

    thenUserCanStillLogin();
}

TEST_P(CloudUserOfflineLogin, invited_user_can_login_after_completing_registration_in_cloud)
{
    givenUserInvitedFromDesktopClient();

    whenUserCompletesRegistrationInCloud();
    whenSystemWentOffline();

    thenInvitedUserCanLoginToTheSystem();
}

INSTANTIATE_TEST_CASE_P(P2pMode, CloudUserOfflineLogin,
    ::testing::Values(TestParams(false), TestParams(true)
));
