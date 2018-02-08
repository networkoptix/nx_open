#include <cctype>
#include <chrono>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>
#include <nx/cloud/cdb/data/account_data.h>

#include "email_manager_mocked.h"
#include "test_setup.h"

namespace nx {
namespace cdb {

class AccountTemporaryCredentials:
    public CdbFunctionalTest
{
public:
    AccountTemporaryCredentials():
        m_expirationPeriod(std::chrono::hours(1))
    {
    }

protected:
    void setTemporaryCredentialsExpirationPeriod(
        std::chrono::seconds expirationPeriod)
    {
        m_expirationPeriod = expirationPeriod;
    }

    void allocateAccountTemporaryCredentials()
    {
        api::TemporaryCredentialsParams params;
        params.timeouts.expirationPeriod = m_expirationPeriod;
        m_temporaryCredentials.push_back(api::TemporaryCredentials());
        ASSERT_EQ(
            api::ResultCode::ok,
            createTemporaryCredentials(
                m_account.email,
                m_account.password,
                params,
                &m_temporaryCredentials.back()));
        ASSERT_EQ(
            m_expirationPeriod,
            m_temporaryCredentials.back().timeouts.expirationPeriod);
    }

    void allocateMultipleTemporaryCredentialsForSingleAccount()
    {
        constexpr int credentialsCount = 2;
        for (int i = 0; i < credentialsCount; ++i)
            allocateAccountTemporaryCredentials();
    }

    void changeAccountPassword()
    {
        std::string account1NewPassword = m_account.password + "new";
        api::AccountUpdateData accountUpdateData;
        accountUpdateData.passwordHa1 = nx_http::calcHa1(
            m_account.email.c_str(),
            moduleInfo().realm.c_str(),
            account1NewPassword.c_str()).constData();
        ASSERT_EQ(
            api::ResultCode::ok,
            updateAccount(m_account.email, m_account.password, accountUpdateData));
    }

    void waitForTemporaryCredentialsToExpire()
    {
        std::this_thread::sleep_for(m_expirationPeriod + std::chrono::seconds(1));
    }

    void assertTemporaryCredentialsAreLowCase()
    {
        for (const auto& credentials: m_temporaryCredentials)
        {
            for (auto c: credentials.login)
                ASSERT_EQ(std::tolower(c), c);

            for (auto c: credentials.password)
                ASSERT_EQ(std::tolower(c), c);
        }
    }

    void assertTemporaryCredentialsCanBeUsedToAuthenticateToVms()
    {
        auto cdbConnection = connection(m_system.id, m_system.authKey);

        for (const auto& credentials: m_temporaryCredentials)
        {
            api::AuthRequest authRequest;
            authRequest.nonce = api::generateCloudNonceBase(m_system.id);
            authRequest.realm = nx::network::AppInfo::realm().toStdString();
            authRequest.username = credentials.login;

            api::ResultCode resultCode = api::ResultCode::ok;
            api::AuthResponse authResponse;
            std::tie(resultCode, authResponse) =
                makeSyncCall<api::ResultCode, api::AuthResponse>(
                    std::bind(
                        &nx::cdb::api::AuthProvider::getAuthenticationResponse,
                        cdbConnection->authProvider(),
                        authRequest,
                        std::placeholders::_1));
            ASSERT_EQ(api::ResultCode::ok, resultCode);
            ASSERT_EQ(m_account.email, authResponse.authenticatedAccountData.accountEmail);

            const auto ha1 = nx_http::calcHa1(
                credentials.login.c_str(),
                nx::network::AppInfo::realm().toStdString().c_str(),
                credentials.password.c_str());
            ASSERT_EQ(
                nx_http::calcIntermediateResponse(
                    ha1,
                    QByteArray::fromStdString(authRequest.nonce)).toStdString(),
                authResponse.intermediateResponse);
        }
    }

    void assertTemporaryCredentialsCannotBeUsedToAuthenticateToVms()
    {
        for (const auto& credentials: m_temporaryCredentials)
        {
            api::AccountData accountData;
            ASSERT_EQ(
                api::ResultCode::notAuthorized,
                getAccount(
                    credentials.login,
                    credentials.password,
                    &accountData));
        }
    }

    void assertTemporaryCredentialsCannotBeUsedPermanentlyToAuthenticateToVms()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (i == 1)
                ASSERT_TRUE(restart());

            assertTemporaryCredentialsCannotBeUsedToAuthenticateToVms();
        }
    }

private:
    AccountWithPassword m_account;
    std::vector<api::TemporaryCredentials> m_temporaryCredentials;
    api::SystemData m_system;
    std::chrono::seconds m_expirationPeriod;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_account = addActivatedAccount2();
        m_system = addRandomSystemToAccount(m_account);
    }
};

TEST_F(AccountTemporaryCredentials, verify_temporary_credentials_are_accepted_by_cdb)
{
    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    for (int i = 0; i < 3; ++i)
    {
        api::TemporaryCredentialsParams params;
        if (i == 0)
            params.timeouts.expirationPeriod = std::chrono::hours(1);
        else if (i == 1)
            params.type = "long";
        else if (i == 2)
            params.type = "short";
        api::TemporaryCredentials temporaryCredentials;
        result = createTemporaryCredentials(
            account1.email,
            account1Password,
            params,
            &temporaryCredentials);
        ASSERT_EQ(api::ResultCode::ok, result);
        if (i == 0)
        {
            ASSERT_EQ(
                params.timeouts.expirationPeriod,
                temporaryCredentials.timeouts.expirationPeriod);
        }
        else if (i == 1)
        {
            ASSERT_EQ(
                std::chrono::hours(24) * 30,
                temporaryCredentials.timeouts.expirationPeriod);
            ASSERT_FALSE(temporaryCredentials.timeouts.autoProlongationEnabled);
        }
        else if (i == 2)
        {
            ASSERT_EQ(
                std::chrono::minutes(60),
                temporaryCredentials.timeouts.expirationPeriod);
            ASSERT_TRUE(temporaryCredentials.timeouts.autoProlongationEnabled);
            ASSERT_EQ(
                std::chrono::minutes(10),
                temporaryCredentials.timeouts.prolongationPeriod);
        }

        for (int j = 0; j < 2; ++j)
        {
            if (j == 1)
            {
                ASSERT_TRUE(restart());
            }

            result = getAccount(
                temporaryCredentials.login,
                temporaryCredentials.password,
                &account1);
            ASSERT_EQ(api::ResultCode::ok, result);
            ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);
        }

        //checking that account update is forbidden
        std::string account1NewPassword = account1Password + "new";
        api::AccountUpdateData update;
        update.passwordHa1 = nx_http::calcHa1(
            account1.email.c_str(),
            moduleInfo().realm.c_str(),
            account1NewPassword.c_str()).constData();
        update.fullName = account1.fullName + "new";
        update.customization = account1.customization + "new";

        result = updateAccount(
            temporaryCredentials.login,
            temporaryCredentials.password,
            update);
        ASSERT_EQ(result, api::ResultCode::forbidden);
    }
}

TEST_F(AccountTemporaryCredentials, temporary_credentials_expiration)
{
    setTemporaryCredentialsExpirationPeriod(std::chrono::seconds(1));

    allocateAccountTemporaryCredentials();
    waitForTemporaryCredentialsToExpire();
    assertTemporaryCredentialsCannotBeUsedPermanentlyToAuthenticateToVms();
}

TEST_F(AccountTemporaryCredentials, temporary_credentials_login_to_system)
{
    allocateAccountTemporaryCredentials();
    assertTemporaryCredentialsCanBeUsedToAuthenticateToVms();
}

TEST_F(
    AccountTemporaryCredentials,
    multiple_temporary_credentials_can_be_used_simultaneously_to_login_to_system)
{
    allocateMultipleTemporaryCredentialsForSingleAccount();
    assertTemporaryCredentialsCanBeUsedToAuthenticateToVms();
}

TEST_F(AccountTemporaryCredentials, temporary_credentials_removed_on_password_change)
{
    allocateAccountTemporaryCredentials();
    changeAccountPassword();
    assertTemporaryCredentialsCannotBeUsedPermanentlyToAuthenticateToVms();
}

TEST_F(AccountTemporaryCredentials, temporary_credentials_are_low_case)
{
    allocateAccountTemporaryCredentials();
    assertTemporaryCredentialsAreLowCase();
}

} // namespace cdb
} // namespace nx
