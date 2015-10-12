/**********************************************************
* Oct 12, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, systemGetCloudUsers)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(addActivatedAccount(&account1, &account1Password), api::ResultCode::ok);

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(addActivatedAccount(&account2, &account2Password), api::ResultCode::ok);

    //adding system1 to account1
    api::SystemData system1;
    ASSERT_EQ(bindRandomSystem(account1.email, account1Password, &system1), api::ResultCode::ok);

    //adding system2 to account1
    api::SystemData system2;
    ASSERT_EQ(bindRandomSystem(account1.email, account1Password, &system2), api::ResultCode::ok);

    //adding system3 to account2
    api::SystemData system3;
    ASSERT_EQ(bindRandomSystem(account2.email, account2Password, &system3), api::ResultCode::ok);

    //sharing system1 with account2 as viewer
    ASSERT_EQ(
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.id,
            api::SystemAccessRole::viewer),
        api::ResultCode::ok);

    //sharing system2 with account2 as editorWithSharing
    ASSERT_EQ(
        shareSystem(
            account1.email,
            account1Password,
            system2.id,
            account2.id,
            api::SystemAccessRole::editorWithSharing),
        api::ResultCode::ok);

    //checking account1 system list
    {
        std::vector<api::SystemData> systems;
        ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 2);
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system2) != systems.end());
    }

    //checking account2 system list
    {
        std::vector<api::SystemData> systems;
        ASSERT_EQ(getSystems(account2.email, account2Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 3);
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system2) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system3) != systems.end());
    }

    //checking sharings from account1 view
    {
        std::vector<api::SystemSharing> sharings;
        ASSERT_EQ(
            getSystemSharings(account1.email, account1Password, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 4);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.id, system1.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.id, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.id, system1.id),
            api::SystemAccessRole::viewer);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.id, system2.id),
            api::SystemAccessRole::editorWithSharing);
    }

    //checking sharings from account2 view
    {
        std::vector<api::SystemSharing> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 3);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.id, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.id, system2.id),
            api::SystemAccessRole::editorWithSharing);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.id, system3.id),
            api::SystemAccessRole::owner);
    }

    //checking sharings of system1 from account2 view
    {
        std::vector<api::SystemSharing> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, system1.id.toStdString(), &sharings),
            api::ResultCode::forbidden);
    }

    //checking sharings of system2 from account2 view
    {
        std::vector<api::SystemSharing> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, system2.id.toStdString(), &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.id, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.id, system2.id),
            api::SystemAccessRole::editorWithSharing);
    }
}

}   //cdb
}   //nx
