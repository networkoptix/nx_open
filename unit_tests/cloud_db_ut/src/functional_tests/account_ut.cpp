/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <functional>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "test_setup.h"
#include "version.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, createAccount)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    addAccount(&account1, &account1Password);

    api::AccountData account2;
    std::string account2Password;
    addAccount(&account2, &account2Password);

    //adding system1 to account1
    api::SystemData system1;
    {
        const auto result = bindRandomSystem(account1.email, account1Password, &system1);
        ASSERT_EQ(result, api::ResultCode::ok);
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
            account2.id,
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
    addAccount(&account3, &account3Password);

    {
        //trying to share system1 from account2 to account3
        const auto result = shareSystem(
            account2.email,
            account2Password,
            system1.id,
            account3.id,
            api::SystemAccessRole::editor);
        ASSERT_EQ(result, api::ResultCode::forbidden);
    }
}

}   //cdb
}   //nx
