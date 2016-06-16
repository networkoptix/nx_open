/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <chrono>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <data/account_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/fusion/model_functions.h>

#include "email_manager_mocked.h"
#include "test_setup.h"

#include <utils/common/app_info.h>


namespace nx {
namespace cdb {

namespace {
class Account
:
    public CdbFunctionalTest
{
};
}

TEST_F(Account, activation)
{
    addArg("--db/inactivityTimeout=5s");

    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(1);

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
    ASSERT_EQ(account1.customization, QnAppInfo::customizationName().toStdString());
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
    ASSERT_EQ(QnAppInfo::customizationName().toStdString(), account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);
}

TEST_F(Account, reactivation)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(2);

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
    ASSERT_EQ(account1.customization, QnAppInfo::customizationName().toStdString());
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
    ASSERT_EQ(QnAppInfo::customizationName().toStdString(), account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);

    //subsequent activation MUST fail
    for (int i = 0; i < 2; ++i)
    {
        std::string email;
        result = activateAccount(activationCode, &email);
        ASSERT_EQ(api::ResultCode::notFound, result);

        if (i == 0)
            restart();
    }
}

//reactivation of already activated account must fail
TEST_F(Account, reactivation_activated_account)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(1);

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
    ASSERT_EQ(QnAppInfo::customizationName().toStdString(), account1.customization);
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
        sendAsyncMocked(QByteArray())).Times(3);

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
        ASSERT_EQ(system1.customization, QnAppInfo::customizationName().toStdString());
    }

    {
        //fetching account1 systems
        std::vector<api::SystemDataEx> systems;
        const auto result = getSystems(account1.email, account1Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 1);
        ASSERT_TRUE(system1 == systems[0]);
    }

    {
        //fetching account2 systems
        std::vector<api::SystemDataEx> systems;
        const auto result = getSystems(account2.email, account2Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 0);
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
        ASSERT_EQ(systems.size(), 1);
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
        ASSERT_EQ(result, api::ResultCode::forbidden);
    }
}

TEST_F(Account, badRegistration)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(1);

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
    auto client = nx_http::AsyncHttpClient::create();
    QUrl url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/cdb/account/register");
    std::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        client.get(), [&donePromise](nx_http::AsyncHttpClientPtr /*client*/) {
            donePromise.set_value();
        },
        Qt::DirectConnection);
    client->doPost(url, "application/json", QJson::serialized(account1));

    doneFuture.wait();
    ASSERT_TRUE(client->response() != nullptr);
    ASSERT_EQ(nx_http::StatusCode::ok, client->response()->statusLine.statusCode);

    bool success = false;
    nx_http::FusionRequestResult requestResult =
        QJson::deserialized<nx_http::FusionRequestResult>(
            client->fetchMessageBodyBuffer(),
            nx_http::FusionRequestResult(),
            &success);
    ASSERT_TRUE(success);
    ASSERT_NE(nx_http::FusionRequestErrorClass::noError, requestResult.errorClass);
}

TEST_F(Account, requestQueryDecode)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    std::string account1Password = "12352";
    api::AccountData account1;
    account1.email = "test%40yandex.ru";
    account1.fullName = "Test Test";
    account1.passwordHa1 = nx_http::calcHa1(
        "test@yandex.ru",
        moduleInfo().realm.c_str(),
        account1Password.c_str()).constData();

    api::AccountConfirmationCode activationCode;
    result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QnAppInfo::customizationName().toStdString());
    ASSERT_TRUE(!activationCode.code.empty());

    std::string activatedAccountEmail;
    result = activateAccount(activationCode, &activatedAccountEmail);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(
        QUrl::fromPercentEncoding(account1.email.c_str()).toStdString(),
        activatedAccountEmail);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::notAuthorized);  //test%40yandex.ru MUST be unknown email

    account1.email = "test@yandex.ru";
    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QnAppInfo::customizationName().toStdString());
    ASSERT_EQ(account1.statusCode, api::AccountStatus::activated);
    ASSERT_EQ(account1.email, "test@yandex.ru");
}

TEST_F(Account, update)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);

    //changing password
    std::string account1NewPassword = account1Password + "new";
    api::AccountUpdateData update;
    update.passwordHa1 = nx_http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    update.fullName = account1.fullName + "new";
    update.customization = account1.customization + "new";

    result = updateAccount(account1.email, account1Password, update);
    ASSERT_EQ(result, api::ResultCode::ok);

    account1.fullName = update.fullName.get();
    account1.customization = update.customization.get();

    api::AccountData newAccount;
    result = getAccount(account1.email, account1NewPassword, &newAccount);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(newAccount, account1);

    restart();

    result = getAccount(account1.email, account1NewPassword, &newAccount);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(newAccount, account1);
}

TEST_F(Account, resetPassword_general)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(4);

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

        if (i == 1)
            restart();  //checking that code is valid after cloud_db restart

        //confirmation code has format base64(tmp_password:email)
        const auto tmpPasswordAndEmail = QByteArray::fromBase64(
            QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
        const std::string tmpPassword = tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

        //setting new password
        std::string account1NewPassword = "new_password";

        api::AccountUpdateData update;
        update.passwordHa1 = nx_http::calcHa1(
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

TEST_F(Account, resetPassword_expiration)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(QByteArray())).Times(2);

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
        if (i == 1)
            restart();

        api::AccountUpdateData update;
        update.passwordHa1 = nx_http::calcHa1(
            account1.email.c_str(),
            moduleInfo().realm.c_str(),
            account1NewPassword.c_str()).constData();
        result = updateAccount(account1.email, tmpPassword, update);
        ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword has expired

        result = getAccount(account1.email, account1Password, &account1);
        ASSERT_EQ(api::ResultCode::ok, result); //old password is still active
    }
}

//checks that password reset code is valid for changing password only
TEST_F(Account, resetPassword_authorization)
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
        if (i == 1)
            restart();

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
    update.passwordHa1 = nx_http::calcHa1(
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

    restart();

    result = updateAccount(account1.email, tmpPassword, update);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword is removed
}

TEST_F(Account, reset_password_activates_account)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    //adding account
    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    api::ResultCode result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QnAppInfo::customizationName().toStdString());
    ASSERT_TRUE(!activationCode.code.empty());

    //user did not activate account and forgot password

    //resetting password
    std::string confirmationCode;
    result = resetAccountPassword(account1.email, &confirmationCode);
    ASSERT_EQ(api::ResultCode::ok, result);

    //confirmation code has format base64(tmp_password:email)
    const auto tmpPasswordAndEmail = QByteArray::fromBase64(
        QByteArray::fromRawData(confirmationCode.data(), confirmationCode.size()));
    const std::string tmpPassword = tmpPasswordAndEmail.mid(0, tmpPasswordAndEmail.indexOf(':')).constData();

    //setting new password
    std::string account1NewPassword = "new_password";

    api::AccountUpdateData update;
    update.passwordHa1 = nx_http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    result = updateAccount(account1.email, tmpPassword, update);
    ASSERT_EQ(api::ResultCode::ok, result);

    result = getAccount(account1.email, account1NewPassword, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);

    restart();

    result = getAccount(account1.email, account1NewPassword, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);
}

TEST_F(Account, temporary_credentials)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    api::TemporaryCredentialsParams params;
    params.expirationPeriod = std::chrono::hours(1);
    api::TemporaryCredentials temporaryCredentials;
    result = createTemporaryCredentials(
        account1.email,
        account1Password,
        params,
        &temporaryCredentials);
    ASSERT_EQ(api::ResultCode::ok, result);

    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
            restart();

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

TEST_F(Account, temporary_credentials_expiration)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    constexpr const auto expirationPeriod = std::chrono::seconds(5);

    api::AccountData account1;
    std::string account1Password;
    api::ResultCode result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    api::TemporaryCredentialsParams params;
    params.expirationPeriod = expirationPeriod;
    api::TemporaryCredentials temporaryCredentials;
    result = createTemporaryCredentials(
        account1.email,
        account1Password,
        params,
        &temporaryCredentials);
    ASSERT_EQ(api::ResultCode::ok, result);

    result = getAccount(
        temporaryCredentials.login,
        temporaryCredentials.password,
        &account1);
    ASSERT_EQ(api::ResultCode::ok, result);

    std::this_thread::sleep_for(expirationPeriod);

    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
            restart();

        result = getAccount(
            temporaryCredentials.login,
            temporaryCredentials.password,
            &account1);
        ASSERT_EQ(api::ResultCode::notAuthorized, result);
    }
}

}   //cdb
}   //nx
