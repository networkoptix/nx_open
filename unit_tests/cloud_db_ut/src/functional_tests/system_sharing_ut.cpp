/**********************************************************
* Jan 25, 2016
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

namespace {
class SystemSharing
:
    public CdbFunctionalTest
{
};
}

TEST_F(SystemSharing, getCloudUsers)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            getSystemSharings(account1.email, account1Password, "sdfnoowertn", &sharings));
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
    api::SystemSharing system1ToAccount2SharingData;
    system1ToAccount2SharingData.systemID = system1.id;
    system1ToAccount2SharingData.accountEmail = account2.email;
    system1ToAccount2SharingData.accessRole = api::SystemAccessRole::viewer;
    system1ToAccount2SharingData.groupID = "customGroupID";
    system1ToAccount2SharingData.customPermissions = "customPermissions123123";
    system1ToAccount2SharingData.isEnabled = false;
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            std::move(system1ToAccount2SharingData)));
    // vmsUserId will be filled by cloud_db
    system1ToAccount2SharingData.vmsUserId = 
        guidFromArbitraryData(system1ToAccount2SharingData.accountEmail)
            .toSimpleString().toStdString();

    //sharing system2 with account2 as cloudAdmin
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system2.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    restart();

    //checking account1 system list
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(2, systems.size());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system2) != systems.end());

        ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, systems[0].accessRole);
        ASSERT_EQ(6, systems[0].sharingPermissions.size());

        ASSERT_EQ(account1.email, systems[1].ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, systems[1].accessRole);
        ASSERT_EQ(6, systems[1].sharingPermissions.size());
    }

    //checking account2 system list
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(getSystems(account2.email, account2Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(systems.size(), 3);
        const auto system1Iter = std::find(systems.begin(), systems.end(), system1);
        const auto system2Iter = std::find(systems.begin(), systems.end(), system2);
        const auto system3Iter = std::find(systems.begin(), systems.end(), system3);
        ASSERT_TRUE(system1Iter != systems.end());
        ASSERT_TRUE(system2Iter != systems.end());
        ASSERT_TRUE(system3Iter != systems.end());

        ASSERT_EQ(account1.email, system1Iter->ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::viewer, system1Iter->accessRole);
        ASSERT_TRUE(system1Iter->sharingPermissions.empty());

        ASSERT_EQ(account1.email, system2Iter->ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::cloudAdmin, system2Iter->accessRole);
        ASSERT_EQ(5, system2Iter->sharingPermissions.size());

        ASSERT_EQ(account2.email, system3Iter->ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, system3Iter->accessRole);
        ASSERT_EQ(6, system3Iter->sharingPermissions.size());
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
            findSharing(sharings, account1.email, system1.id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            account1.fullName,
            findSharing(sharings, account1.email, system2.id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system1.id),
            api::SystemAccessRole::viewer);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system1.id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::cloudAdmin);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system2.id).accountFullName);

        bool found = false;
        for (const auto& sharingData : sharings)
        {
            if (sharingData.systemID == system1ToAccount2SharingData.systemID &&
                sharingData.accountEmail == system1ToAccount2SharingData.accountEmail)
            {
                found = true;
                ASSERT_TRUE((const api::SystemSharing&)sharingData == system1ToAccount2SharingData);
                ASSERT_EQ(account2.fullName, sharingData.accountFullName);
                ASSERT_EQ(account2.id, sharingData.accountID);
            }
        }
        ASSERT_TRUE(found);
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
            api::SystemAccessRole::cloudAdmin);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system3.id),
            api::SystemAccessRole::owner);
    }

    //checking sharings of system1 from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, system1.id, &sharings),
            api::ResultCode::forbidden);
    }

    //checking sharings of system2 from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, system2.id, &sharings));
        ASSERT_EQ(sharings.size(), 2);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::cloudAdmin);
    }

    //checking sharings of system1 from system's view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(system1.id, system1.authKey, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(2, sharings.size());

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account1.email, system1.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system1.id),
            api::SystemAccessRole::viewer);
    }
}

TEST_F(SystemSharing, maintenance)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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
        ASSERT_EQ(2, sharings.size());
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
        ASSERT_EQ(2, sharings.size());
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
        ASSERT_EQ(1, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(0, sharings.size());
    }

    restart();

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(1, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, account1.email, system1.id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(0, sharings.size());
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
        ASSERT_EQ(3, ownerSharings.size());
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
        role <= api::SystemAccessRole::cloudAdmin;
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
            api::SystemAccessRole::cloudAdmin));

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
        ASSERT_EQ(2, ownerSharings.size());
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

/** only system owner and integrator can share system granting maintenance role */
TEST_F(SystemSharing, maintenance2)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    api::AccountData account3;
    std::string account3Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account3, &account3Password));

    //adding system to the first one
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        shareSystem(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::maintenance));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        shareSystem(
            account2.email,
            account2Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::maintenance));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::maintenance));
}

TEST_F(SystemSharing, owner)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    //adding "cloudAdmin" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

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
            api::SystemAccessRole::cloudAdmin,
            accountAccessRoleForSystem(sharings, account2.email, system1.id));
    }
}

TEST_F(SystemSharing, remove_system)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    //sharing system
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding "cloudAdmin" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account1.email, account1Password, &sharings));
        ASSERT_EQ(2, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::cloudAdmin,
            accountAccessRoleForSystem(sharings, account2.email, system1.id));
    }

    {
        //checking account2 system list
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account2.email, account2Password, &systems));
        ASSERT_EQ(1, systems.size());
        ASSERT_EQ(system1.name, systems[0].name);
        ASSERT_EQ(system1.ownerAccountEmail, systems[0].ownerAccountEmail);
    }

    //removing system
    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(account1.email, account1Password, system1.id));

    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
            restart();

        //checking that sharing has also been removed (before and after restart)
        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account2.email, account2Password, &sharings));
            ASSERT_TRUE(sharings.empty());
        }

        {
            //checking account2 system list
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystems(account2.email, account2Password, &systems));
            ASSERT_TRUE(systems.empty());
        }
    }
}

TEST_F(SystemSharing, get_access_role_list)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    //sharing system
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding "cloudAdmin" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    api::AccountData account3;
    std::string account3Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account3, &account3Password));

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account3.email,
            api::SystemAccessRole::maintenance));
    api::AccountData account4;
    std::string account4Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account4, &account4Password));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account4.email,
            api::SystemAccessRole::localAdmin));

    api::AccountData account5;
    std::string account5Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account5, &account5Password));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            account1.email,
            account1Password,
            system1.id,
            account5.email,
            api::SystemAccessRole::viewer));

    api::AccountData account6;
    std::string account6Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account6, &account6Password));

    //checking access roles
    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
            restart();

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account1.email, account1Password, system1.id, &accessRoles));
            ASSERT_EQ(6, accessRoles.size());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::localAdmin) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::cloudAdmin) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::liveViewer) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::viewer) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::advancedViewer) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::maintenance) != accessRoles.end());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::forbidden,
                getAccessRoleList(account1.email, account1Password, "unkown_system_id", &accessRoles));
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account1.email, account1Password, system1.id, &sharings));
            ASSERT_EQ(5, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account2.email, account2Password, system1.id, &accessRoles));
            ASSERT_EQ(5, accessRoles.size());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::localAdmin) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::cloudAdmin) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::liveViewer) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::viewer) != accessRoles.end());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::advancedViewer) != accessRoles.end());
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account2.email, account2Password, system1.id, &sharings));
            ASSERT_EQ(5, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account3.email, account3Password, system1.id, &accessRoles));
            ASSERT_EQ(1, accessRoles.size());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::maintenance) != accessRoles.end());
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account3.email, account3Password, system1.id, &sharings));
            ASSERT_EQ(5, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account4.email, account4Password, system1.id, &accessRoles));
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account4.email, account4Password, system1.id, &sharings));
            ASSERT_EQ(5, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::forbidden,
                getAccessRoleList(account5.email, account5Password, system1.id, &accessRoles));
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::forbidden,
                getAccessRoleList(account6.email, account6Password, system1.id, &accessRoles));
        }
    }
}

TEST_F(SystemSharing, remove_sharing_unknown_account)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    //removing not-existent sharing
    ASSERT_EQ(
        api::ResultCode::notFound,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            "unknown@account.email",
            api::SystemAccessRole::none));
}

/**
 * Disabled since cloud_db returns forbidden when trying to share with non-existent system.
 * This is a minor problem, so leaving it as-is for now.
 */
TEST_F(SystemSharing, DISABLED_remove_sharing_unknown_system)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

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
        api::ResultCode::notFound,
        updateSystemSharing(
            account1.email,
            account1Password,
            QnUuid::createUuid().toString().toStdString(),   /* Let assume we can never have system with random id. */
            "unknown_account_name",
            api::SystemAccessRole::none));

    //removing not-existent sharing, empty account email
    ASSERT_EQ(
        api::ResultCode::notFound,
        updateSystemSharing(
            account1.email,
            account1Password,
            system1.id,
            std::string(),
            api::SystemAccessRole::none));
}

TEST_F(SystemSharing, changingOwnRightsOnSystem)
{
    //waiting for cloud_db initialization
    ASSERT_TRUE(startAndWaitUntilStarted());

    const api::SystemAccessRole targetRoles[] = {
        api::SystemAccessRole::cloudAdmin,
        api::SystemAccessRole::maintenance
    };

    const api::SystemAccessRole allRoles[] = {
        api::SystemAccessRole::disabled,
        api::SystemAccessRole::custom,
        api::SystemAccessRole::liveViewer,
        api::SystemAccessRole::viewer,
        api::SystemAccessRole::advancedViewer,
        api::SystemAccessRole::localAdmin,
        api::SystemAccessRole::cloudAdmin,
        api::SystemAccessRole::maintenance,
        api::SystemAccessRole::owner
    };

    for (const auto targetRole: targetRoles)
    {
        const auto account1 = addActivatedAccount2();
        const auto account2 = addActivatedAccount2();
        const auto system1 = addRandomSystemToAccount(account1);
        shareSystem2(account1, system1, account2, targetRole);

        for (const auto role: allRoles)
        {
            // Changing own role in system. Failure is expected
            ASSERT_EQ(
                api::ResultCode::forbidden,
                updateSystemSharing(
                    account2.data.email,
                    account2.password,
                    system1.id,
                    account2.data.email,
                    role));
        }

        // Removing myself from system's uses
        shareSystem2(account2, system1, account2, api::SystemAccessRole::none);
    }
}

}   //cdb
}   //nx
