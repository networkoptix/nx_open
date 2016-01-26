/**********************************************************
* Jan 25, 2016
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, system_sharing_getCloudUsers)
{
    //waiting for cloud_db initialization
    startAndWaitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account1.email, account1Password, "sdfnoowertn", &sharings),
            api::ResultCode::forbidden);
    }

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding system1 to account1
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    //adding system2 to account1
    api::SystemData system2;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system2));

    //adding system3 to account2
    api::SystemData system3;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account2.email, account2Password, &system3));

    //sharing system1 with account2 as viewer
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::viewer));

    //sharing system2 with account2 as editorWithSharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system2.id,
            account2.email,
            api::SystemAccessRole::editorWithSharing));

    restart();

    //checking account1 system list
    {
        std::vector<api::SystemData> systems;
        ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 2);
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system2) != systems.end());
        ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
        ASSERT_EQ(account1.email, systems[1].ownerAccountEmail);
    }

    //checking account2 system list
    {
        std::vector<api::SystemData> systems;
        ASSERT_EQ(getSystems(account2.email, account2Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 3);
        const auto system1Iter = std::find(systems.begin(), systems.end(), system1);
        const auto system2Iter = std::find(systems.begin(), systems.end(), system2);
        const auto system3Iter = std::find(systems.begin(), systems.end(), system3);
        ASSERT_TRUE(system1Iter != systems.end());
        ASSERT_TRUE(system2Iter != systems.end());
        ASSERT_TRUE(system3Iter != systems.end());

        ASSERT_EQ(account1.email, system1Iter->ownerAccountEmail);
        ASSERT_EQ(account1.email, system2Iter->ownerAccountEmail);
        ASSERT_EQ(account2.email, system3Iter->ownerAccountEmail);
    }

    //checking sharings from account1 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account1.email, account1Password, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 4);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system1.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            account1.fullName,
            findSharing(sharings, account1.email, system1.id).fullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            account1.fullName,
            findSharing(sharings, account1.email, system2.id).fullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system1.id),
            api::SystemAccessRole::viewer);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system1.id).fullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::editorWithSharing);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system2.id).fullName);
    }

    //checking sharings from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 3);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::editorWithSharing);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system3.id),
            api::SystemAccessRole::owner);
    }

    //checking sharings of system1 from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, system1.id.toStdString(), &sharings),
            api::ResultCode::forbidden);
    }

    //checking sharings of system2 from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, system2.id.toStdString(), &sharings));
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::editorWithSharing);
    }
}

TEST_F(CdbFunctionalTest, system_sharing_maintenance)
{
    //waiting for cloud_db initialization
    startAndWaitUntilStarted();

    //creating two accounts
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding system to the first one
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::maintenance));

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(sharings, account2.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(sharings, account2.email, system1.id));
    }

    restart();

    //account1: trying to modify (change access rights) sharing to account2 
    //(failure: maintenance sharing cannot be updated)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::viewer));

    //account1: trying to remove sharing to account2 
    //(failure: maintenance sharing cannot be removed)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::none));

    //account2: trying to modify sharing to account2 
    //(failure: integrator cannot modify its sharing, only remove)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::viewer));

    //account2: trying to remove "owner" sharing to account1 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::none));
    //account2: trying to alter "owner" sharing to account1 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::viewer));
    //account1: trying to remove "owner" sharing to itself (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::none));

    //account2: trying to remove sharing to account2 
    //(success: itegrator removed sharing to itself)
    ASSERT_EQ(
        api::ResultCode::ok,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::none));

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(sharings.size(), 1);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(sharings.size(), 0);
    }

    restart();

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(sharings.size(), 1);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(sharings.size(), 0);
    }

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::maintenance));

    //adding another maintenance
    api::AccountData account3;
    std::string account3Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account3, &account3Password));

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::maintenance));

    //checking system list for both accounts
    std::vector<api::SystemSharingEx> ownerSharings;
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &ownerSharings));
        ASSERT_EQ(ownerSharings.size(), 3);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(ownerSharings, account1.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account2.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account3.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_TRUE(sharings == ownerSharings);
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account3.email, account3Password, &sharings));
        ASSERT_TRUE(sharings == ownerSharings);
    }

    //adding another maintenance
    api::AccountData account4;
    std::string account4Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account4, &account4Password));

    for (auto role = api::SystemAccessRole::owner;
    role <= api::SystemAccessRole::editorWithSharing;
        role = static_cast<api::SystemAccessRole>(static_cast<int>(role) + 1))
    {
        if (role == api::SystemAccessRole::maintenance)
            continue;

        ASSERT_EQ(
            api::ResultCode::forbidden,
            shareSystem(
                account2.email,
                account2Password,
                system1.id,
                account4.email,
                role));
    }

    //account2: trying to upgrade sharing to account3 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::editorWithSharing));

    //account2: removing sharing to account3 (success)
    ASSERT_EQ(
        api::ResultCode::ok,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::none));

    //checking system list for both accounts
    {
        ownerSharings.clear();
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &ownerSharings));
        ASSERT_EQ(ownerSharings.size(), 2);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(ownerSharings, account1.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account2.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_TRUE(sharings == ownerSharings);
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account3.email, account3Password, &sharings));
        ASSERT_TRUE(sharings.empty());
    }
}

TEST_F(CdbFunctionalTest, system_sharing_owner)
{
    //waiting for cloud_db initialization
    startAndWaitUntilStarted();

    //creating account
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding system to account
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    //adding "editorWithSharing" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::editorWithSharing));

    //trying to update "owner" sharing
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::viewer));
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::viewer));

    //trying to remove "owner" sharing
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::none));
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system1.id,
            account1.email,
            api::SystemAccessRole::none));

    //checking system list for account 1
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
        ASSERT_EQ(
            api::SystemAccessRole::editorWithSharing,
            accountAccessRoleForSystem(sharings, account2.email, system1.id));
    }
}

TEST_F(CdbFunctionalTest, DISABLED_system_sharing_remove_unknown_sharing)
{
    //waiting for cloud_db initialization
    startAndWaitUntilStarted();

    //creating two accounts
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    //adding system to account
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    //removing not-existent sharing to unknown system
    ASSERT_EQ(
        api::ResultCode::badRequest,
        updateSystemSharing(
            account1.email,
            account1Password,
            "unknown_system_id",
            "unknown_account_name",
            api::SystemAccessRole::none));

    //removing not-existent sharing, empty account email
    ASSERT_EQ(
        api::ResultCode::badRequest,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            std::string(),
            api::SystemAccessRole::none));

    //removing not-existent sharing
    ASSERT_EQ(
        api::ResultCode::badRequest,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            "unknown_account_name",
            api::SystemAccessRole::none));
}

}   //cdb
}   //nx
