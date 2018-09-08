#include <chrono>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/network/connection_server/user_locker.h>
#include <nx/utils/app_info.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>
#include <nx/cloud/cdb/data/account_data.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "email_manager_mocked.h"
#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

namespace {

class Account:
    public CdbFunctionalTest
{
};

} // namespace

// TODO: #ak following test should be broken into something like:
//
//{
//    whenIssuedAccountRegistrationRequest();
//    assertAccountActivationNotificationHasBeenSent();
//    assertAccountActivationCodeWorks();
//}
//
//{
//    whenIssuedAccountRegistrationRequest();
//    assertAccountCannotBindSystem();
//}

TEST_F(Account, activation)
{
    addArg("--db/inactivityTimeout=5s");

    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(1);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, nx::utils::AppInfo::customizationName().toStdString());
    ASSERT_TRUE(!activationCode.code.empty());

    //only /account/activate and /account/reactivate are allowed for not activated account

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::accountNotActivated, result);

    //waiting for DB connection to timeout
    std::this_thread::sleep_for(std::chrono::seconds(10));

    api::AccountData account1Tmp;
    result = getAccount(account1.email, account1Password, &account1Tmp);
    ASSERT_EQ(api::ResultCode::accountNotActivated, result);

    std::string email;
    result = activateAccount(activationCode, &email);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(account1.email, email);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(nx::utils::AppInfo::customizationName().toStdString(), account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);
}

TEST_F(Account, reactivation)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(2);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, nx::utils::AppInfo::customizationName().toStdString());
    ASSERT_TRUE(!activationCode.code.empty());

    //reactivating account (e.g. we lost activation code)
    api::AccountConfirmationCode activationCode2;
    result = reactivateAccount(account1.email, &activationCode2);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_TRUE(!activationCode2.code.empty());

    ASSERT_EQ(activationCode.code, activationCode2.code);

    std::string email;
    result = activateAccount(activationCode, &email);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(account1.email, email);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(nx::utils::AppInfo::customizationName().toStdString(), account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);

    //subsequent activation MUST fail
    for (int i = 0; i < 2; ++i)
    {
        std::string email;
        result = activateAccount(activationCode, &email);
        ASSERT_EQ(api::ResultCode::notFound, result);

        if (i == 0)
        {
            ASSERT_TRUE(restart());
        }
    }
}

//reactivation of already activated account must fail
TEST_F(Account, reactivation_activated_account)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(1);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;
    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(nx::utils::AppInfo::customizationName().toStdString(), account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);

    //reactivating account (e.g. we lost activation code)
    api::AccountConfirmationCode activationCode;
    result = reactivateAccount(account1.email, &activationCode);
    ASSERT_EQ(result, api::ResultCode::forbidden);
}

TEST_F(Account, general)
{
    EmailManagerMocked mockedEmailManager;

    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(
            GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(3);

    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(
            GMOCK_DYNAMIC_TYPE_MATCHER(const SystemSharedNotification&))
    ).Times(2);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    api::AccountData account2;
    std::string account2Password;
    result = addActivatedAccount(&account2, &account2Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    //adding system1 to account1
    api::SystemData system1;
    {
        const auto result = bindRandomSystem(account1.email, account1Password, &system1);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(system1.customization, nx::utils::AppInfo::customizationName().toStdString());
    }

    {
        //fetching account1 systems
        std::vector<api::SystemDataEx> systems;
        const auto result = getSystems(account1.email, account1Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(1U, systems.size());
        ASSERT_TRUE(system1 == systems[0]);
    }

    {
        //fetching account2 systems
        std::vector<api::SystemDataEx> systems;
        const auto result = getSystems(account2.email, account2Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 0U);
    }

    {
        //sharing system to account2 with "localAdmin" permission
        const auto result = shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::localAdmin);
        ASSERT_EQ(result, api::ResultCode::ok);
    }

    {
        //fetching account2 systems
        std::vector<api::SystemDataEx> systems;
        const auto result = getSystems(account2.email, account2Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 1U);
        ASSERT_TRUE(system1 == systems[0]);
    }

    //adding account3
    api::AccountData account3;
    std::string account3Password;
    result = addActivatedAccount(&account3, &account3Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    {
        //trying to share system1 from account2 to account3
        const auto result = shareSystem(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::localAdmin);
        ASSERT_EQ(result, api::ResultCode::ok);
    }
}

TEST_F(Account, bad_registration)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(1);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    //trying to add account with same email once again
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::alreadyExists);

    //checking correct error message
    auto client = nx::network::http::AsyncHttpClient::create();
    nx::utils::Url url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/cdb/account/register");
    nx::utils::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx::network::http::AsyncHttpClient::done,
        client.get(),
        [&donePromise](nx::network::http::AsyncHttpClientPtr /*client*/) { donePromise.set_value(); },
        Qt::DirectConnection);
    client->doPost(url, "application/json", QJson::serialized(account1));

    doneFuture.wait();
    ASSERT_TRUE(client->response() != nullptr);
    ASSERT_EQ(nx::network::http::StatusCode::notFound, client->response()->statusLine.statusCode);

    bool success = false;
    nx::network::http::FusionRequestResult requestResult =
        QJson::deserialized<nx::network::http::FusionRequestResult>(
            client->fetchMessageBodyBuffer(),
            nx::network::http::FusionRequestResult(),
            &success);
    ASSERT_TRUE(success);
    ASSERT_NE(nx::network::http::FusionRequestErrorClass::noError, requestResult.errorClass);
}

TEST_F(Account, request_query_decode)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    std::string account1Password = "12352";
    api::AccountData account1;
    account1.email = "test%40yandex.ru";
    account1.fullName = "Test Test";
    account1.passwordHa1 = nx::network::http::calcHa1(
        "test@yandex.ru",
        moduleInfo().realm.c_str(),
        account1Password.c_str()).constData();
    account1.customization = nx::utils::AppInfo::customizationName().toStdString();

    nx::network::http::HttpClient httpClient;
    nx::utils::Url url(
        lm("http://127.0.0.1:%1/cdb/account/register?email=%2&fullName=%3&passwordHa1=%4&customization=%5")
        .arg(endpoint().port).arg(account1.email).arg(account1.fullName)
        .arg(account1.passwordHa1).arg(nx::utils::AppInfo::customizationName().toStdString()));
    ASSERT_TRUE(httpClient.doGet(url));
    ASSERT_EQ(nx::network::http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
    QByteArray responseBody;
    while (!httpClient.eof())
        responseBody += httpClient.fetchMessageBodyBuffer();
    api::AccountConfirmationCode activationCode;
    activationCode = QJson::deserialized<api::AccountConfirmationCode>(responseBody);
    ASSERT_TRUE(!activationCode.code.empty());

    std::string activatedAccountEmail;
    ASSERT_EQ(api::ResultCode::ok, activateAccount(activationCode, &activatedAccountEmail));
    ASSERT_EQ(
        nx::utils::Url::fromPercentEncoding(account1.email.c_str()).toStdString(),
        activatedAccountEmail);

    // test%40yandex.ru MUST be unknown email.
    ASSERT_EQ(
        api::ResultCode::badUsername,
        getAccount(account1.email, account1Password, &account1));

    account1.email = "test@yandex.ru";
    ASSERT_EQ(api::ResultCode::ok, getAccount(account1.email, account1Password, &account1));
    ASSERT_EQ(account1.customization, nx::utils::AppInfo::customizationName().toStdString());
    ASSERT_EQ(account1.statusCode, api::AccountStatus::activated);
    ASSERT_EQ(account1.email, "test@yandex.ru");
}

TEST_F(Account, update)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    auto account1 = addActivatedAccount2();

    //changing password
    std::string account1NewPassword = account1.password + "new";
    api::AccountUpdateData update;
    update.passwordHa1 = nx::network::http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    update.passwordHa1Sha256 = nx::network::http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str(),
        "SHA-256").constData();
    update.fullName = account1.fullName + "new";
    update.customization = account1.customization + "new";

    ASSERT_EQ(
        api::ResultCode::ok,
        updateAccount(account1.email, account1.password, update));

    account1.password = account1NewPassword;
    account1.fullName = update.fullName.get();
    account1.customization = update.customization.get();

    bool restarted = false;
    for (;;)
    {
        api::AccountData newAccount;
        ASSERT_EQ(
            api::ResultCode::ok,
            getAccount(account1.email, account1.password, &newAccount));
        ASSERT_EQ(account1, newAccount);

        if (restarted)
            break;

        ASSERT_TRUE(restart());
        restarted = true;
    }
}

TEST_F(Account, reset_password_general)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(2);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const RestorePasswordNotification&))
    ).Times(2);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    ASSERT_TRUE(startAndWaitUntilStarted());

    for (int i = 0; i < 2; ++i)
    {
        //adding account
        api::AccountData account1;
        std::string account1Password;
        api::ResultCode result = addActivatedAccount(&account1, &account1Password);
        ASSERT_EQ(api::ResultCode::ok, result);

        //resetting passsword
        std::string confirmationCode;
        result = resetAccountPassword(account1.email, &confirmationCode);
        ASSERT_EQ(api::ResultCode::ok, result);

        //old password is still active
        result = getAccount(account1.email, account1Password, &account1);
        ASSERT_EQ(api::ResultCode::ok, result);

        if (i > 0)
        {
            ASSERT_TRUE(restart());  //checking that code is valid after cloud_db restart
        }

        //confirmation code has format base64(tmp_password:email)
        const auto tmpPasswordAndEmail = QByteArray::fromBase64(
            QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
        const std::string tmpPassword = tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

        //setting new password
        std::string account1NewPassword = "new_password";

        api::AccountUpdateData update;
        update.passwordHa1 = nx::network::http::calcHa1(
            account1.email.c_str(),
            moduleInfo().realm.c_str(),
            account1NewPassword.c_str()).constData();
        result = updateAccount(account1.email, tmpPassword, update);
        ASSERT_EQ(api::ResultCode::ok, result);

        result = getAccount(account1.email, account1NewPassword, &account1);
        ASSERT_EQ(api::ResultCode::ok, result);

        //temporary password must have already been removed
        result = updateAccount(account1.email, tmpPassword, update);
        ASSERT_EQ(api::ResultCode::notAuthorized, result);
    }
}

TEST_F(Account, reset_password_expiration)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(1);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const RestorePasswordNotification&))
    ).Times(1);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/) {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    const std::chrono::seconds expirationPeriod(5);

    addArg("-accountManager/passwordResetCodeExpirationTimeout");
    addArg(QByteArray::number((unsigned int)expirationPeriod.count()).constData());

    ASSERT_TRUE(startAndWaitUntilStarted());

    //adding account
    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    //resetting passsword
    std::string confirmationCode;
    result = resetAccountPassword(account1.email, &confirmationCode);
    ASSERT_EQ(api::ResultCode::ok, result);

    //waiting for temporary password to expire
    std::this_thread::sleep_for(expirationPeriod + std::chrono::seconds(1));

    //confirmation code has format base64(tmp_password:email)
    const auto tmpPasswordAndEmail = QByteArray::fromBase64(
        QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
    const std::string tmpPassword = tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

    //setting new password
    std::string account1NewPassword = "new_password";

    for (int i = 0; i < 2; ++i)
    {
        if (i > 0)
        {
            ASSERT_TRUE(restart());
        }

        api::AccountUpdateData update;
        update.passwordHa1 = nx::network::http::calcHa1(
            account1.email.c_str(),
            moduleInfo().realm.c_str(),
            account1NewPassword.c_str()).constData();
        result = updateAccount(account1.email, tmpPassword, update);
        ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword has expired

        result = getAccount(account1.email, account1Password, &account1);
        ASSERT_EQ(api::ResultCode::ok, result); //old password is still active
    }
}

TEST_F(Account, reset_password_links_expiration_after_changing_password)
{
    ASSERT_TRUE(startAndWaitUntilStarted());
    auto account1 = addActivatedAccount2();

    // Password has been lost.
    account1.password.clear();

    // Resetting passsword.
    std::string confirmationCode1;
    ASSERT_EQ(
        api::ResultCode::ok,
        resetAccountPassword(account1.email, &confirmationCode1));

    std::string confirmationCode2;
    ASSERT_EQ(
        api::ResultCode::ok,
        resetAccountPassword(account1.email, &confirmationCode2));

    for (int i = 0; i < 3; ++i)
    {
        // First run - changing password using confirmationCode2.
        // Second run - testing confirmationCode1 does not work anymore.
        // Third run - same as the second one, but with restart
        const auto& confirmationCode = i == 0 ? confirmationCode2 : confirmationCode1;

        // Confirmation code has format base64(tmp_password:email).
        const auto tmpPasswordAndEmail = QByteArray::fromBase64(
            QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
        const std::string tmpPassword =
            tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

        // Setting new account password.
        account1.password = "new_password";

        api::AccountUpdateData update;
        update.passwordHa1 = nx::network::http::calcHa1(
            account1.email.c_str(),
            moduleInfo().realm.c_str(),
            account1.password.c_str()).constData();
        const auto result = updateAccount(account1.email, tmpPassword, update);
        if (i == 0)
        {
            ASSERT_EQ(api::ResultCode::ok, result);
            // Checking new password works fine
            api::AccountData accountData;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccount(account1.email, account1.password, &accountData));
        }
        else
        {
            // Checking that confirmation code 1 cannot be used anymore
            ASSERT_NE(api::ResultCode::ok, result);
        }

        if (i == 1)
        {
            ASSERT_TRUE(restart());
        }
    }
}

//checks that password reset code is valid for changing password only
TEST_F(Account, reset_password_authorization)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    //adding account
    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    api::AccountData account2;
    std::string account2Password;
    result = addActivatedAccount(&account2, &account2Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    api::SystemData system1;
    result = bindRandomSystem(
        account1.email,
        account1Password,
        &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    //resetting passsword
    std::string confirmationCode;
    result = resetAccountPassword(account1.email, &confirmationCode);
    ASSERT_EQ(api::ResultCode::ok, result);

    //confirmation code has format base64(tmp_password:email)
    const auto tmpPasswordAndEmail = QByteArray::fromBase64(
        QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
    const std::string tmpPassword = tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

    for (int i = 0; i < 2; ++i)
    {
        if (i > 0)
        {
            ASSERT_TRUE(restart());
        }

        //verifying that only /account/update is allowed with this temporary password
        api::AccountData accountData;
        result = getAccount(account1.email, tmpPassword, &accountData);
        ASSERT_EQ(api::ResultCode::forbidden, result);

        api::SystemData system2;
        result = bindRandomSystem(
            account1.email,
            tmpPassword,
            &system2);
        ASSERT_EQ(api::ResultCode::forbidden, result);

        std::vector<api::SystemDataEx> systems;
        result = getSystems(account1.email, tmpPassword, &systems);
        ASSERT_EQ(api::ResultCode::forbidden, result);

        std::vector<api::SystemSharingEx> sharings;
        result = getSystemSharings(account1.email, tmpPassword, &sharings);
        ASSERT_EQ(api::ResultCode::forbidden, result);

        {
            //sharing system to account2 with "localAdmin" permission
            const auto result = shareSystem(
                account1.email,
                tmpPassword,
                system1.id,
                account2.email,
                api::SystemAccessRole::localAdmin);
            ASSERT_EQ(api::ResultCode::forbidden, result);
        }
    }

    //setting new password
    std::string account1NewPassword = "new_password";

    api::AccountUpdateData update;
    update.passwordHa1 = nx::network::http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    result = updateAccount(account1.email, tmpPassword, update);
    ASSERT_EQ(api::ResultCode::ok, result);  //tmpPassword is still active

    api::AccountData accountData;
    result = getAccount(account1.email, account1NewPassword, &accountData);
    ASSERT_EQ(api::ResultCode::ok, result); //new password has been set

    result = updateAccount(account1.email, tmpPassword, update);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword is removed

    ASSERT_TRUE(restart());

    result = updateAccount(account1.email, tmpPassword, update);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword is removed
}

TEST_F(Account, created_while_sharing)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(2);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const InviteUserNotification&))
    ).Times(1);

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/)
        {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account1 = addActivatedAccount2();
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1.password, &system1));

    const std::string newAccountEmail = generateRandomEmailAddress();
    std::string newAccountPassword;

    const auto newAccountAccessRoleInSystem1 = api::SystemAccessRole::cloudAdmin;
    shareSystemEx(account1, system1, newAccountEmail, newAccountAccessRoleInSystem1);

    // Ignoring invitation and registering account in a regular way

    data::AccountData newAccount;
    newAccount.email = newAccountEmail;
    api::AccountConfirmationCode accountConfirmationCode;
    ASSERT_EQ(
        api::ResultCode::ok,
        addAccount(&newAccount, &newAccountPassword, &accountConfirmationCode));
    std::string resultEmail;
    ASSERT_EQ(
        api::ResultCode::ok,
        activateAccount(accountConfirmationCode, &resultEmail));

    for (int i = 0; i < 2; ++i)
    {
        if (i > 0)
        {
            ASSERT_TRUE(restart());
        }

        ASSERT_EQ(
            api::ResultCode::ok,
            getAccount(newAccountEmail, newAccountPassword, &newAccount));
        ASSERT_EQ(api::AccountStatus::activated, newAccount.statusCode);
    }

    ASSERT_EQ(newAccountEmail, resultEmail);

    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(newAccountEmail, newAccountPassword, &systems));
    ASSERT_EQ(1U, systems.size());
    ASSERT_EQ(system1.id, systems[0].id);
    ASSERT_EQ(newAccountAccessRoleInSystem1, systems[0].accessRole);
}

//-------------------------------------------------------------------------------------------------

class AccountNewTest:
    public Account
{
public:
    static constexpr std::chrono::seconds kTimeShift = std::chrono::hours(3);

    AccountNewTest():
        m_timeShift(nx::utils::test::ClockType::system)
    {
    }

    ~AccountNewTest()
    {
        if (m_emailManagerFactoryBak)
            EMailManagerFactory::setFactory(std::move(*m_emailManagerFactoryBak));
    }

protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_emailManager.setOnReceivedNotification(
            std::bind(&AccountNewTest::notificationReceived, this, _1));

        m_emailManagerFactoryBak = EMailManagerFactory::setFactory(
            [this](const conf::Settings& /*settings*/)
            {
                return std::make_unique<EmailManagerStub>(&m_emailManager);
            });

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    virtual void notificationReceived(const nx::cdb::AbstractNotification& /*notification*/)
    {
    }

    void givenNotActivatedAccount()
    {
        m_registrationTimeRange.first =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        auto result = addAccount(&m_account, &m_account.password, &m_activationCode);
        ASSERT_EQ(api::ResultCode::ok, result);
        ASSERT_TRUE(!m_activationCode.code.empty());
        m_registrationTimeRange.second =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
    }

    void givenActivatedAccount()
    {
        givenNotActivatedAccount();
        whenActivateAccount();
    }

    void useWrongAccountPassword()
    {
        m_passwordToUse = nx::utils::generateRandomName(7).toStdString();
    }

    void useUnknownAccountLogin()
    {
        m_usernameToUse = BusinessDataGenerator::generateRandomEmailAddress();
    }

    void whenShiftedSystemTime()
    {
        m_timeShift.applyRelativeShift(kTimeShift);
    }

    void whenActivateAccount()
    {
        m_activationTimeRange.first =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        std::string accountEmail;
        auto result = activateAccount(m_activationCode, &accountEmail);
        ASSERT_EQ(api::ResultCode::ok, result);
        m_activationTimeRange.second =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
    }

    void whenRestoreAccountPassword()
    {
        std::string confirmationCode;
        ASSERT_EQ(
            api::ResultCode::ok,
            resetAccountPassword(m_account.email, &confirmationCode));

        // Confirmation code has format base64(tmp_password:email).
        const auto tmpPasswordAndEmail = QByteArray::fromBase64(
            QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
        const std::string tmpPassword =
            tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

        const std::string newPassword = nx::utils::generateRandomName(7).toStdString();

        api::AccountUpdateData update;
        update.passwordHa1 = nx::network::http::calcHa1(
            m_account.email.c_str(),
            moduleInfo().realm.c_str(),
            newPassword.c_str()).constData();
        ASSERT_EQ(api::ResultCode::ok, updateAccount(m_account.email, tmpPassword, update));

        m_account.password = newPassword;
        m_account.passwordHa1 = *update.passwordHa1;
    }

    void whenRestartedCloudDb()
    {
        ASSERT_TRUE(restart());
    }

    void whenRequestAccountInfo()
    {
        api::AccountData accountData;
        m_prevResultCode = getAccount(
            m_usernameToUse ? *m_usernameToUse : m_account.email,
            m_passwordToUse ? *m_passwordToUse : m_account.password,
            &accountData);
    }

    void whenRequestSystemList()
    {
        std::vector<api::SystemDataEx> systems;
        m_prevResultCode = getSystems(m_account.email, m_account.password, &systems);
    }

    void assertRegistrationTimestampIsCorrect()
    {
        using namespace std::chrono;

        const auto account = getFreshAccountCopy();
        ASSERT_GE(
            nx::utils::floor<milliseconds>(account.registrationTime),
            nx::utils::floor<milliseconds>(m_registrationTimeRange.first));
        ASSERT_LE(
            nx::utils::floor<milliseconds>(account.registrationTime),
            nx::utils::floor<milliseconds>(m_registrationTimeRange.second));
    }

    void assertActivationTimestampIsCorrect()
    {
        using namespace std::chrono;

        const auto account = getFreshAccountCopy();
        ASSERT_GE(
            nx::utils::floor<milliseconds>(account.activationTime),
            nx::utils::floor<milliseconds>(m_activationTimeRange.first));
        ASSERT_LE(
            nx::utils::floor<milliseconds>(account.activationTime),
            nx::utils::floor<milliseconds>(m_activationTimeRange.second));
    }

    void thenResultCodeIs(api::ResultCode expectedResultCode)
    {
        ASSERT_TRUE(m_prevResultCode);
        ASSERT_EQ(expectedResultCode, *m_prevResultCode);
    }

    const AccountWithPassword& account() const
    {
        return m_account;
    }

    AccountWithPassword& account()
    {
        return m_account;
    }

    api::AccountConfirmationCode lastActivationCode() const
    {
        return m_activationCode;
    }

private:
    using TimeRange =
        std::pair<std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>;

    nx::utils::test::ScopedTimeShift m_timeShift;
    AccountWithPassword m_account;
    api::AccountConfirmationCode m_activationCode;
    TimeRange m_registrationTimeRange;
    TimeRange m_activationTimeRange;
    std::optional<api::ResultCode> m_prevResultCode;
    std::optional<std::string> m_usernameToUse;
    std::optional<std::string> m_passwordToUse;
    TestEmailManager m_emailManager;
    std::optional<EMailManagerFactory::FactoryFunc> m_emailManagerFactoryBak;

    api::AccountData getFreshAccountCopy()
    {
        api::AccountData account;
        auto result = getAccount(m_account.email, m_account.password, &account);
        NX_GTEST_ASSERT_EQ(api::ResultCode::ok, result);
        return account;
    }
};

constexpr std::chrono::seconds AccountNewTest::kTimeShift;

TEST_F(AccountNewTest, account_timestamps)
{
    givenNotActivatedAccount();

    whenShiftedSystemTime();

    whenActivateAccount();

    assertRegistrationTimestampIsCorrect();
    assertActivationTimestampIsCorrect();

    whenRestartedCloudDb();

    assertRegistrationTimestampIsCorrect();
    assertActivationTimestampIsCorrect();
}

//-------------------------------------------------------------------------------------------------

class AccountPasswordReset:
    public AccountNewTest
{
    using base_type = AccountNewTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenActivatedAccount();
    }

    void setLargeLoginExistenceConcealDelay()
    {
        args().erase(
            std::remove_if(
                args().begin(), args().end(),
                [](char* arg) { return strstr(arg, "accountManager/loginExistenceConcealDelay") != nullptr; }),
            args().end());

        addArg("--accountManager/loginExistenceConcealDelay=1h");
        whenRestartedCloudDb();
    }

    void whenRestorePassword()
    {
        using namespace std::placeholders;

        m_cdbConnection = connection("", "");
        m_cdbConnection->accountManager()->resetPassword(
            {account().email},
            std::bind(&AccountPasswordReset::saveResetPasswordResult, this, _1, _2));
    }

    void thenResponseIsNotReceived()
    {
        static constexpr std::chrono::milliseconds kWaitPeriod = std::chrono::seconds(1);

        ASSERT_FALSE(m_resetPasswordResults.pop(kWaitPeriod));
    }

private:
    using Result = std::tuple<api::ResultCode, api::AccountConfirmationCode>;

    std::unique_ptr<nx::cdb::api::Connection> m_cdbConnection;
    nx::utils::SyncQueue<Result> m_resetPasswordResults;

    void saveResetPasswordResult(
        api::ResultCode resultCode,
        api::AccountConfirmationCode confirmationCode)
    {
        m_resetPasswordResults.push(
            std::make_tuple(resultCode, std::move(confirmationCode)));
    }
};

TEST_F(AccountPasswordReset, password_reset_response_is_delayed_based_on_settings)
{
    setLargeLoginExistenceConcealDelay();

    whenRestorePassword();
    thenResponseIsNotReceived();
}

//-------------------------------------------------------------------------------------------------

class AccountFailedAuthenticationResultCodes:
    public AccountNewTest
{
};

TEST_F(
    AccountFailedAuthenticationResultCodes,
    proper_error_code_is_reported_when_using_not_activated_account)
{
    givenNotActivatedAccount();

    whenRequestAccountInfo();
    thenResultCodeIs(api::ResultCode::accountNotActivated);

    whenRequestSystemList();
    thenResultCodeIs(api::ResultCode::accountNotActivated);
}

TEST_F(
    AccountFailedAuthenticationResultCodes,
    unauthorized_is_reported_when_existing_user_uses_wrong_password)
{
    givenActivatedAccount();

    useWrongAccountPassword();
    whenRequestAccountInfo();

    thenResultCodeIs(api::ResultCode::notAuthorized);
}

TEST_F(
    AccountFailedAuthenticationResultCodes,
    badUsername_is_reported_when_using_unknown_username)
{
    useUnknownAccountLogin();
    whenRequestAccountInfo();
    thenResultCodeIs(api::ResultCode::badUsername);
}

//-------------------------------------------------------------------------------------------------

class AccountActivation:
    public AccountNewTest
{
    using base_type = AccountNewTest;

protected:
    void whenUpdateAccountUsingActivationCode()
    {
        const auto codeParts = QByteArray::fromBase64(
            lastActivationCode().code.c_str()).split(':');
        ASSERT_EQ(2, codeParts.size());

        api::AccountUpdateData update;
        const std::string password = nx::utils::generateRandomName(7).toStdString();
        update.fullName = nx::utils::generateRandomName(7).toStdString();
        update.passwordHa1 = nx::network::http::calcHa1(
            codeParts[1].toStdString().c_str(),
            nx::network::AppInfo::realm().toStdString().c_str(),
            password.c_str()).toStdString();
        ASSERT_EQ(
            api::ResultCode::ok,
            updateAccount(codeParts[1].toStdString(), codeParts[0].toStdString(), update));

        account().password = password;
    }

    void assertActivationCodeContainsEmail()
    {
        const auto codeParts = QByteArray::fromBase64(
            lastActivationCode().code.c_str()).split(':');
        ASSERT_EQ(2, codeParts.size());

        ASSERT_EQ(account().email, codeParts[1].toStdString());
    }

    void thenAccountIsActivated()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (i > 0)
                whenRestartedCloudDb();

            api::AccountData accountData;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccount(account().email, account().password, &accountData));
            ASSERT_EQ(api::AccountStatus::activated, accountData.statusCode);

            constexpr auto maxAllowedDiff = std::chrono::hours(1);
            ASSERT_GT(accountData.activationTime, nx::utils::utcTime() - maxAllowedDiff);
            ASSERT_LT(accountData.activationTime, nx::utils::utcTime() + maxAllowedDiff);
        }
    }

    void assertNotificationContainsFullName()
    {
        ASSERT_EQ(
            account().fullName,
            m_activateNotifications.back().message.userFullName);
    }

private:
    std::vector<ActivateAccountNotification> m_activateNotifications;

    virtual void notificationReceived(
        const nx::cdb::AbstractNotification& notification) override
    {
        const auto activateNotification =
            dynamic_cast<const ActivateAccountNotification*>(&notification);
        if (activateNotification)
            m_activateNotifications.push_back(*activateNotification);
    }
};

TEST_F(
    AccountActivation,
    activation_code_contains_account_email)
{
    givenNotActivatedAccount();
    assertActivationCodeContainsEmail();
}

TEST_F(
    AccountActivation,
    activation_code_can_be_used_to_activate_account_with_update_account_call)
{
    givenNotActivatedAccount();
    whenUpdateAccountUsingActivationCode();
    thenAccountIsActivated();
}

TEST_F(AccountActivation, account_can_be_activated_by_password_reset)
{
    givenNotActivatedAccount();
    whenRestoreAccountPassword();
    thenAccountIsActivated();
}

TEST_F(AccountActivation, activate_notification_contains_full_name)
{
    givenNotActivatedAccount();
    assertNotificationContainsFullName();
}

//-------------------------------------------------------------------------------------------------

class AccountInvite:
    public AccountNewTest
{
    using base_type = AccountNewTest;

public:
    AccountInvite():
        m_timeShift(nx::utils::test::ClockType::system)
    {
    }

protected:
    void whenInvitedSameNotRegisteredUserToMultipleSystems()
    {
        for (auto& system: m_systems)
        {
            ASSERT_EQ(
                api::ResultCode::ok,
                shareSystem(account(), system.id,
                    m_newAccountEmail, api::SystemAccessRole::cloudAdmin));
        }
    }

    void inviteUser()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(account(), m_systems.front().id,
                m_newAccountEmail, api::SystemAccessRole::cloudAdmin));
    }

    void waitForInviteLinkToExpire()
    {
        // TODO: Replace with corresponding setting value (probably, doubled).
        m_timeShift.applyAbsoluteShift(std::chrono::hours(24) * 7);
    }

    void removeUserFromSystem()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            removeSystemSharing(
                account().email,
                account().password,
                m_systems.front().id,
                m_newAccountEmail));
    }

    void thenSameInviteCodeHasBeenDelivered()
    {
        ASSERT_EQ(m_systems.size(), m_inviteNotifications.size());
        std::set<std::string> inviteCodes;
        for (const auto& notification: m_inviteNotifications)
        {
            inviteCodes.insert(notification.message.code);
            ASSERT_EQ(1U, inviteCodes.size());
        }
    }

    void assertInviteCodeContainsEmail()
    {
        const auto codeParts = QByteArray::fromBase64(
            m_inviteNotifications.back().message.code.c_str()).split(':');
        ASSERT_EQ(2, codeParts.size());

        ASSERT_EQ(m_newAccountEmail, codeParts[1].toStdString());
    }

    void assertAccountCanBeActivatedUsingLatestInviteLink()
    {
        const auto codeParts = QByteArray::fromBase64(
            m_inviteNotifications.back().message.code.c_str()).split(':');

        api::AccountUpdateData update;
        const std::string password = nx::utils::generateRandomName(7).toStdString();
        update.passwordHa1 = nx::network::http::calcHa1(
            m_newAccountEmail.c_str(),
            nx::network::AppInfo::realm().toStdString().c_str(),
            password.c_str()).toStdString();
        ASSERT_EQ(
            api::ResultCode::ok,
            updateAccount(
                codeParts[1].toStdString(),
                codeParts[0].toStdString(),
                update));
    }

private:
    std::string m_newAccountEmail;
    std::vector<api::SystemData> m_systems;
    std::vector<InviteUserNotification> m_inviteNotifications;
    nx::utils::test::ScopedTimeShift m_timeShift;

    virtual void SetUp() override
    {
        constexpr const std::size_t kSystemCount = 7;

        base_type::SetUp();

        m_newAccountEmail = BusinessDataGenerator::generateRandomEmailAddress();

        givenNotActivatedAccount();
        whenActivateAccount();

        m_systems.resize(kSystemCount);
        for (auto& system: m_systems)
            system = addRandomSystemToAccount(account());
    }

    virtual void notificationReceived(
        const nx::cdb::AbstractNotification& notification) override
    {
        const auto inviteNotification =
            dynamic_cast<const InviteUserNotification*>(&notification);
        if (inviteNotification)
            m_inviteNotifications.push_back(*inviteNotification);
    }
};

TEST_F(AccountInvite, invite_code_contains_email)
{
    inviteUser();
    assertInviteCodeContainsEmail();
}

TEST_F(AccountInvite, inviting_same_user_to_different_systems_produce_same_invite_link)
{
    whenInvitedSameNotRegisteredUserToMultipleSystems();
    thenSameInviteCodeHasBeenDelivered();
}

TEST_F(AccountInvite, repeating_invite_after_previous_invite_link_has_expired_is_ok)
{
    inviteUser();
    waitForInviteLinkToExpire();
    removeUserFromSystem();

    inviteUser();

    assertAccountCanBeActivatedUsingLatestInviteLink();
}

//-------------------------------------------------------------------------------------------------

class AccountLockout:
    public AccountNewTest
{
public:
    AccountLockout():
        m_wrongPassword(QnUuid::createUuid().toStdString()),
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    void givenBlockedAccount()
    {
        givenActivatedAccount();
        whenDoNumberOfRequestsWithWrongPassword();
        thenAccountIsBlocked();
    }

    void whenDoNumberOfRequestsWithWrongPassword()
    {
        for (int i = 0; i < m_settings.authFailureCount; ++i)
        {
            api::AccountData response;
            getAccount(account().email, m_wrongPassword, &response);
        }
    }

    void whenLockPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_settings.lockPeriod);
    }

    void thenAccountIsBlocked()
    {
        api::AccountData response;
        ASSERT_EQ(
            api::ResultCode::accountBlocked,
            getAccount(account().email, account().password, &response));
    }

    void thenAccountIsUnlocked()
    {
        api::AccountData response;
        ASSERT_EQ(
            api::ResultCode::ok,
            getAccount(account().email, account().password, &response));
    }

protected:
    nx::network::server::UserLockerSettings m_settings;

private:
    std::string m_wrongPassword;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

class AccountLockoutEnabled:
    public AccountLockout
{
public:
    AccountLockoutEnabled()
    {
        m_settings.lockPeriod = std::chrono::hours(1);

        addArg("--loginLockout/enabled=true");

        auto str = lm("--loginLockout/lockPeriod=%1")
            .args(m_settings.lockPeriod).toStdString();
        addArg(str.c_str());
    }
};

TEST_F(
    AccountLockoutEnabled,
    account_is_locked_out_after_number_of_requests_with_wrong_password)
{
    givenActivatedAccount();
    whenDoNumberOfRequestsWithWrongPassword();
    thenAccountIsBlocked();
}

TEST_F(AccountLockoutEnabled, account_is_unlocked_when_lock_period_passes)
{
    givenBlockedAccount();
    whenLockPeriodPasses();
    thenAccountIsUnlocked();
}

//-------------------------------------------------------------------------------------------------

class AccountLockoutDisabled:
    public AccountLockout
{
public:
    AccountLockoutDisabled()
    {
        addArg("--loginLockout/enabled=false");
    }
};

TEST_F(AccountLockoutDisabled, account_is_not_locked_if_disabled)
{
    givenActivatedAccount();
    whenDoNumberOfRequestsWithWrongPassword();
    thenAccountIsUnlocked();
}

} // namespace test
} // namespace cdb
} // namespace nx
