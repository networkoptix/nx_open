#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/cloud/db/test_support/business_data_generator.h>
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
    void givenCloudSystem()
    {
        ASSERT_TRUE(startMediaServer());

        connectSystemToCloud();

        waitUserCloudAuthInfoToBeSynchronized();
    }

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
                nx::cloud::db::api::SystemAccessRole::viewer);
        }
    }

    void whenVmsLostConnectionToTheCloud()
    {
        waitForCloudDataSynchronizedToTheMediaServer();

        cdb()->stop();

        whenMediaServerRestarted();
    }

    void whenSystemGoesOffline()
    {
        cdb()->stop();
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
            nx::cloud::db::api::ResultCode::ok,
            cdb()->getAccount(
                m_invitedAccount.email,
                m_invitedAccount.password,
                &m_invitedAccount));
    }

    void whenUserRegistersInCloudSkippingInviteEmail()
    {
        m_invitedAccount = nx::cloud::db::test::BusinessDataGenerator::generateRandomAccount(
            m_invitedUserEc2Data.email.toStdString());

        waitForUserToAppearInCloud(m_invitedAccount.email);

        m_prevActivationCode = nx::cloud::db::api::AccountConfirmationCode();
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            cdb()->addAccount(&m_invitedAccount, &m_invitedAccount.password, &*m_prevActivationCode));
    }

    void whenUserActivatesAccountByFollowingActivationLink()
    {
        ASSERT_TRUE(static_cast<bool>(m_prevActivationCode));
        std::string accountEmail;
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
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
                [](auto param) { return param.name == nx::cloud::db::api::kVmsUserAuthInfoAttributeName; });
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

    void thenCloudOwnerCanLogin()
    {
        ASSERT_EQ(ec2::ErrorCode::ok, checkIfOwnerCanLogin());
    }

    void thenUserCanStillLogin()
    {
        waitUntilUserLoginFuncPasses([this]() { return checkIfOwnerCanLogin(); });
    }

    void thenAllUsersCanStillLogin()
    {
        for (const auto& account: m_additionalCloudUsers)
        {
            nx::network::http::Credentials credentials(
                account.email.c_str(),
                nx::network::http::PasswordAuthToken(account.password.c_str()));

            waitUntilUserLoginFuncPasses(
                [this, credentials]()
                {
                    return checkIfUserCanLogin(credentials);
                });
        }
    }

    void thenInvitedUserCanLoginToTheSystemEventually()
    {
        waitUntilUserLoginFuncPasses([this](){ return checkInvitedUserLoginToVms(); });
    }

    void thenInvitedUserCannotLoginToTheSystem()
    {
        ASSERT_NE(ec2::ErrorCode::ok, checkInvitedUserLoginToVms());
    }

private:
    std::vector<nx::cloud::db::AccountWithPassword> m_additionalCloudUsers;
    nx::vms::api::UserData m_invitedUserEc2Data;
    nx::cloud::db::AccountWithPassword m_invitedAccount;
    boost::optional<nx::cloud::db::api::AccountConfirmationCode> m_prevActivationCode;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startCloudDB());
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
            if (resourceParamPresent(params, nx::cloud::db::api::kVmsUserAuthInfoAttributeName))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void changeAccountPassword(const std::string& email, const std::string& password)
    {
        std::string confirmationCode;
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            cdb()->resetAccountPassword(email, &confirmationCode));

        nx::cloud::db::api::AccountUpdateData accountUpdate;
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
            nx::cloud::db::api::ResultCode::ok,
            cdb()->updateAccount(email, tmpPassword, accountUpdate));
    }

    ec2::ErrorCode checkInvitedUserLoginToVms()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        mediaServerClient->setUserCredentials(nx::network::http::Credentials(
            m_invitedAccount.email.c_str(),
            nx::network::http::PasswordAuthToken(m_invitedAccount.password.c_str())));
        nx::vms::api::UserDataList users;
        return mediaServerClient->ec2GetUsers(&users);
    }

    ec2::ErrorCode checkIfUserCanLogin(
        const nx::network::http::Credentials& credentials)
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        mediaServerClient->setUserCredentials(credentials);

        nx::vms::api::UserDataList users;
        return mediaServerClient->ec2GetUsers(&users);
    }

    ec2::ErrorCode checkIfOwnerCanLogin()
    {
        auto mediaServerClient = prepareMediaServerClientFromCloudOwner();
        nx::vms::api::ResourceParamDataList vmsSettings;
        return mediaServerClient->ec2GetSettings(&vmsSettings);
    }

    template<typename CheckUserLoginFunc>
    void waitUntilUserLoginFuncPasses(CheckUserLoginFunc checkUserLoginFunc)
    {
        for (;;)
        {
            const auto result = checkUserLoginFunc();
            switch (result)
            {
                case ec2::ErrorCode::ok:
                    return;

                case ec2::ErrorCode::unauthorized:
                case ec2::ErrorCode::forbidden:
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;

                default:
                    FAIL() << toString(result).toStdString();
                    break;
            }
        }
    }
};

//-------------------------------------------------------------------------------------------------

TEST_P(CloudUserOfflineLogin, DISABLED_offline_login_works_just_after_binding_system_to_the_cloud)
{
    mediaServer().addSetting("delayBeforeSettingMasterFlag", "1h");

    ASSERT_TRUE(startMediaServer());
    connectSystemToCloud();

    whenSystemGoesOffline();
    thenCloudOwnerCanLogin();
}

TEST_P(CloudUserOfflineLogin, login_works_on_offline_server_after_restart)
{
    givenCloudSystem();
    whenVmsLostConnectionToTheCloud();
    thenUserCanStillLogin();
}

TEST_P(CloudUserOfflineLogin, multiple_users_can_login)
{
    givenCloudSystem();

    whenAddedMultipleCloudUsers();
    whenVmsLostConnectionToTheCloud();

    thenAllUsersCanStillLogin();
}

// TEST_P(CloudUserOfflineLogin, multiple_users_different_history_depth)

// TEST_P(CloudUserOfflineLogin, cloud_user_added_while_system_is_offline_cannot_login_while_others_can)

TEST_P(CloudUserOfflineLogin, user_can_login_after_password_change)
{
    givenCloudSystem();

    whenCloudUserPasswordHasBeenChanged();
    whenVmsLostConnectionToTheCloud();

    thenUserCanStillLogin();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_user_can_login_after_registering_by_following_invite_link)
{
    givenCloudSystem();
    givenUserInvitedFromDesktopClient();

    whenUserCompletesRegistrationInCloud();
    whenVmsLostConnectionToTheCloud();

    thenInvitedUserCanLoginToTheSystemEventually();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_user_can_login_after_missing_invite_email_and_completing_registration_in_cloud)
{
    givenCloudSystem();
    givenUserInvitedFromDesktopClient();

    whenUserRegistersInCloudSkippingInviteEmail();
    whenUserActivatesAccountByFollowingActivationLink();
    whenVmsLostConnectionToTheCloud();

    thenInvitedUserCanLoginToTheSystemEventually();
}

TEST_P(
    CloudUserOfflineLogin,
    invited_and_registered_but_not_activated_user_cannot_login_to_the_system)
{
    givenCloudSystem();
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
    givenCloudSystem();

    waitForUserCloudAuthInfoToBeUpdated();
    whenVmsLostConnectionToTheCloud();

    thenUserCanStillLogin();
}

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TEST_CASE_P(P2pMode, CloudUserOfflineLoginAfterAuthInfoUpdate,
    ::testing::Values(TestParams(false), TestParams(true)
));
