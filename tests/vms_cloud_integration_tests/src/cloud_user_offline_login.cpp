#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/cloud/cdb/client/data/auth_data.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/string.h>
#include <nx/utils/std/optional.h>

#include "mediaserver_cloud_integration_test_setup.h"

namespace {

static bool resourceParamPresent(
    const nx::vms::api::ResourceParamDataList& params,
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
    using base_type = MediaServerCloudIntegrationTest;

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

    void whenVmsLostConnectionToTheCloud()
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

    void whenUserRegistersInCloudSkippingInviteEmail()
    {
        m_invitedAccount = nx::cdb::test::BusinessDataGenerator::generateRandomAccount(
            m_invitedUserEc2Data.email.toStdString());

        waitForUserToAppearInCloud(m_invitedAccount.email);

        m_prevActivationCode = nx::cdb::api::AccountConfirmationCode();
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->addAccount(&m_invitedAccount, &m_invitedAccount.password, &*m_prevActivationCode));
    }

    void whenUserActivatesAccountByFollowingActivationLink()
    {
        ASSERT_TRUE(static_cast<bool>(m_prevActivationCode));
        std::string accountEmail;
        ASSERT_EQ(
            nx::cdb::api::ResultCode::ok,
            cdb()->activateAccount(*m_prevActivationCode, &accountEmail));
    }

    void waitForUserCloudAuthInfoToBeUpdated()
    {
        std::optional<QString> initialAuthInfo;

        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            nx::vms::api::ResourceParamDataList params;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                mediaServerClient->ec2GetResourceParams(
                    QnUuid::fromStringSafe(cloudOwnerVmsUserId()),
                    &params));
            auto authInfoIter = std::find_if(params.begin(), params.end(),
                [](auto param) { return param.name == nx::cdb::api::kVmsUserAuthInfoAttributeName; });
            if (authInfoIter != params.end())
            {
                if (!initialAuthInfo)
                    initialAuthInfo = authInfoIter->value;
                else if (*initialAuthInfo != authInfoIter->value)
                    break; //< Auth info has been changed.
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
    }

    void thenUserCanStillLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        nx::vms::api::ResourceParamDataList vmsSettings;
        ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient->ec2GetSettings(&vmsSettings));
    }

    void thenAllUsersCanStillLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        for (const auto& account: m_additionalCloudUsers)
        {
            mediaServerClient->setUserCredentials(nx::network::http::Credentials(
                account.email.c_str(),
                nx::network::http::PasswordAuthToken(account.password.c_str())));

            ec2::ApiUserDataList users;
            ASSERT_EQ(ec2::ErrorCode::ok, mediaServerClient->ec2GetUsers(&users));
        }
    }

    void thenInvitedUserCanLoginToTheSystem()
    {
        ASSERT_EQ(ec2::ErrorCode::ok, checkInvitedUserLoginToVms());
    }

    void thenInvitedUserCannotLoginToTheSystem()
    {
        ASSERT_NE(ec2::ErrorCode::ok, checkInvitedUserLoginToVms());
    }

private:
    std::vector<nx::cdb::AccountWithPassword> m_additionalCloudUsers;
    ::ec2::ApiUserData m_invitedUserEc2Data;
    nx::cdb::AccountWithPassword m_invitedAccount;
    boost::optional<nx::cdb::api::AccountConfirmationCode> m_prevActivationCode;

    virtual void SetUp() override
    {
        base_type::SetUp();

        connectSystemToCloud();
        waitUserCloudAuthInfoToBeSynchronized();
    }

    void waitUserCloudAuthInfoToBeSynchronized()
    {
        auto mediaServerClient = prepareMediaServerClient();
        for (;;)
        {
            nx::vms::api::ResourceParamDataList params;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                mediaServerClient->ec2GetResourceParams(
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
        accountUpdate.passwordHa1 = nx::network::http::calcHa1(
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

    ec2::ErrorCode checkInvitedUserLoginToVms()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        mediaServerClient->setUserCredentials(nx::network::http::Credentials(
            m_invitedAccount.email.c_str(),
            nx::network::http::PasswordAuthToken(m_invitedAccount.password.c_str())));
        ec2::ApiUserDataList users;
        return mediaServerClient->ec2GetUsers(&users);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_P(CloudUserOfflineLogin, login_works_on_offline_server_after_restart)
{
    whenVmsLostConnectionToTheCloud();
    thenUserCanStillLogin();
}

TEST_P(CloudUserOfflineLogin, multiple_users_can_login)
{
    whenAddedMultipleCloudUsers();
    whenVmsLostConnectionToTheCloud();

    thenAllUsersCanStillLogin();
}

// TEST_P(CloudUserOfflineLogin, multiple_users_different_history_depth)

// TEST_P(CloudUserOfflineLogin, cloud_user_added_while_system_is_offline_cannot_login_while_others_can)

TEST_P(CloudUserOfflineLogin, user_can_login_after_password_change)
{
    whenCloudUserPasswordHasBeenChanged();
    whenVmsLostConnectionToTheCloud();

    thenUserCanStillLogin();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_user_can_login_after_registering_by_following_invite_link)
{
    givenUserInvitedFromDesktopClient();

    whenUserCompletesRegistrationInCloud();
    whenVmsLostConnectionToTheCloud();

    thenInvitedUserCanLoginToTheSystem();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_user_can_login_after_missing_invite_email_and_completing_registration_in_cloud)
{
    givenUserInvitedFromDesktopClient();

    whenUserRegistersInCloudSkippingInviteEmail();
    whenUserActivatesAccountByFollowingActivationLink();
    whenVmsLostConnectionToTheCloud();

    thenInvitedUserCanLoginToTheSystem();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_and_registered_but_not_activated_user_cannot_login_to_the_system)
{
    givenUserInvitedFromDesktopClient();

    whenUserRegistersInCloudSkippingInviteEmail();
    // User does not follow activation link from corresponding email.
    whenVmsLostConnectionToTheCloud();

    thenInvitedUserCannotLoginToTheSystem();
}

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TEST_CASE_P(P2pMode, CloudUserOfflineLogin,
    ::testing::Values(TestParams(false), TestParams(true)
));

//-------------------------------------------------------------------------------------------------

class CloudUserOfflineLoginAfterAuthInfoUpdate:
    public CloudUserOfflineLogin
{
public:
    CloudUserOfflineLoginAfterAuthInfoUpdate()
    {
        cdb()->addArg("--auth/offlineUserHashValidityPeriod=1s");
        cdb()->addArg("--auth/checkForExpiredAuthPeriod=100ms");
        cdb()->addArg("--auth/continueUpdatingExpiredAuthPeriod=100ms");
    }
};

TEST_P(CloudUserOfflineLoginAfterAuthInfoUpdate, user_is_able_to_login_after_auth_info_update)
{
    waitForUserCloudAuthInfoToBeUpdated();
    whenVmsLostConnectionToTheCloud();
    thenUserCanStillLogin();
}

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TEST_CASE_P(P2pMode, CloudUserOfflineLoginAfterAuthInfoUpdate,
    ::testing::Values(TestParams(false), TestParams(true)
));
