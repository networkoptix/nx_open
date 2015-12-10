/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <chrono>
#include <functional>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <data/account_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <utils/common/model_functions.h>

#include "test_setup.h"
#include "version.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, account_activation)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_TRUE(!activationCode.code.empty());

    //only get_account is allowed for not activated account

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::forbidden, result);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(QN_CUSTOMIZATION_NAME, account1.customization);
    ASSERT_EQ(api::AccountStatus::awaitingActivation, account1.statusCode);

    result = activateAccount(activationCode);
    ASSERT_EQ(api::ResultCode::ok, result);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result);
    ASSERT_EQ(QN_CUSTOMIZATION_NAME, account1.customization);
    ASSERT_EQ(api::AccountStatus::activated, account1.statusCode);
}

TEST_F(CdbFunctionalTest, account_general)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

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
        ASSERT_EQ(system1.customization, QN_CUSTOMIZATION_NAME);
    }

    {
        //fetching account1 systems
        std::vector<api::SystemData> systems;
        const auto result = getSystems(account1.email, account1Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 1);
        ASSERT_TRUE(system1 == systems[0]);
    }

    {
        //fetching account2 systems
        std::vector<api::SystemData> systems;
        const auto result = getSystems(account2.email, account2Password, &systems);
        ASSERT_EQ(result, api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 0);
    }

    {
        //sharing system to account2 with "editor" permission
        const auto result = shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::editor);
        ASSERT_EQ(result, api::ResultCode::ok);
    }

    {
        //fetching account2 systems
        std::vector<api::SystemData> systems;
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
            api::SystemAccessRole::editor);
        ASSERT_EQ(result, api::ResultCode::forbidden);
    }
}

TEST_F(CdbFunctionalTest, account_badRegistration)
{
    waitUntilStarted();

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
    url.setPath("/account/register");
    std::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        client.get(), [&donePromise](nx_http::AsyncHttpClientPtr client) {
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
    ASSERT_NE(nx_http::FusionRequestErrorClass::noError, requestResult.resultCode);
}

TEST_F(CdbFunctionalTest, account_requestQueryDecode)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

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
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_TRUE(!activationCode.code.empty());

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::notAuthorized);  //test%40yandex.ru MUST be unknown email

    account1.email = "test@yandex.ru";
    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_EQ(account1.statusCode, api::AccountStatus::awaitingActivation);
    ASSERT_EQ(account1.email, "test@yandex.ru");
}

TEST_F(CdbFunctionalTest, account_update)
{
    waitUntilStarted();

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
}

TEST_F(CdbFunctionalTest, account_resetPassword_general)
{
    waitUntilStarted();

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

//checks that password reset code is valid for changing password only
TEST_F(CdbFunctionalTest, DISABLED_account_resetPassword_authorization)
{
    waitUntilStarted();

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

    //verifying that only /account/update is allowed with this temporary password
    result = getAccount(account1.email, tmpPassword, &account1);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);

    api::SystemData system2;
    result = bindRandomSystem(
        account1.email,
        tmpPassword,
        &system2);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);

    std::vector<api::SystemData> systems;
    result = getSystems(account1.email, tmpPassword, &systems);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);

    std::vector<api::SystemSharing> sharings;
    result = getSystemSharings(account1.email, tmpPassword, &sharings);
    ASSERT_EQ(api::ResultCode::notAuthorized, result);

    {
        //sharing system to account2 with "editor" permission
        const auto result = shareSystem(
            account1.email,
            tmpPassword,
            system1.id,
            account2.email,
            api::SystemAccessRole::editor);
        ASSERT_EQ(api::ResultCode::notAuthorized, result);
    }
}

TEST_F(CdbFunctionalTest, account_resetPassword_expiration)
{
    const std::chrono::seconds expiraionPeriod(10);
    //TODO #ak setting expiration period

    waitUntilStarted();

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
    std::this_thread::sleep_for(expiraionPeriod + std::chrono::seconds(1));

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
    ASSERT_EQ(api::ResultCode::notAuthorized, result);  //tmpPassword has expired

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(api::ResultCode::ok, result); //old password is still active
}

TEST_F(CdbFunctionalTest, usingPostMethod)
{
    const QByteArray testData =
        "{\"fullName\": \"a k\", \"passwordHa1\": \"5f6291102209098cf5432a415e26d002\", "
        "\"email\": \"andreyk07@gmail.com\", \"customization\": \"default\"}";

    bool success = false;
    auto accountData = QJson::deserialized<data::AccountData>(
        testData, data::AccountData(), &success);
    ASSERT_TRUE(success);

    waitUntilStarted();

    auto client = nx_http::AsyncHttpClient::create();
    QUrl url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/account/register");
    std::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done, 
        client.get(), [&donePromise](nx_http::AsyncHttpClientPtr client) {
            donePromise.set_value();
        },
        Qt::DirectConnection);
    client->doPost(url, "application/json", testData);

    doneFuture.wait();
    ASSERT_TRUE(client->response() != nullptr);
    ASSERT_EQ(nx_http::StatusCode::ok, client->response()->statusLine.statusCode);
}

}   //cdb
}   //nx
