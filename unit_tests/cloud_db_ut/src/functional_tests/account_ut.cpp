/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <functional>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <utils/network/http/auth_tools.h>

#include "test_setup.h"
#include "version.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, accountActivation)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    api::AccountActivationCode activationCode;
    result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_TRUE(!activationCode.code.empty());

    //only get_account is allowed for not activated account

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(result, api::ResultCode::forbidden);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_EQ(account1.statusCode, api::AccountStatus::awaitingActivation);

    result = activateAccount(activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);

    result = getAccount(account1.email, account1Password, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
    ASSERT_EQ(account1.customization, QN_CUSTOMIZATION_NAME);
    ASSERT_EQ(account1.statusCode, api::AccountStatus::activated);
}

TEST_F(CdbFunctionalTest, generalTest)
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

TEST_F(CdbFunctionalTest, badAccountRegistration)
{
    waitUntilStarted();

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    //trying to add account with same email once again
    result = addActivatedAccount(&account1, &account1Password);
    //TODO #ak error code must be alreadyExists
    ASSERT_EQ(result, api::ResultCode::forbidden);
}

TEST_F(CdbFunctionalTest, requestQueryDecode)
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
        account1Password.c_str());

    api::AccountActivationCode activationCode;
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

}   //cdb
}   //nx
