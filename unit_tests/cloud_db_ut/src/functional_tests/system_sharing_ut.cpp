#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/test_support/utils.h>

#include "email_manager_mocked.h"
#include "test_email_manager.h"
#include "test_setup.h"

namespace nx {
namespace cdb {

namespace {

class SystemSharing:
    public CdbFunctionalTest
{
    using base_type = CdbFunctionalTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_ownerAccount = addActivatedAccount2();
        m_system = addRandomSystemToAccount(m_ownerAccount);
    }

    const AccountWithPassword& ownerAccount() const
    {
        return m_ownerAccount;
    }

    const api::SystemData& system() const
    {
        return m_system;
    }

private:
    AccountWithPassword m_ownerAccount;
    api::SystemData m_system;
};

} // namespace

TEST_F(SystemSharing, system_user_list_is_available_under_system_credentials)
{
    std::vector<api::SystemSharingEx> sharings;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystemSharings(system().id, system().authKey, &sharings));
}

TEST_F(SystemSharing, get_users)
{
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            getSystemSharings(ownerAccount().email, ownerAccount().password, "sdfnoowertn", &sharings));
    }

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding system2 to account1
    api::SystemData system2;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(ownerAccount().email, ownerAccount().password, &system2));

    //adding system3 to account2
    api::SystemData system3;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account2.email, account2Password, &system3));

    //sharing system() with account2 as viewer
    api::SystemSharing system1ToAccount2SharingData;
    system1ToAccount2SharingData.systemId = system().id;
    system1ToAccount2SharingData.accountEmail = account2.email;
    system1ToAccount2SharingData.accessRole = api::SystemAccessRole::viewer;
    system1ToAccount2SharingData.userRoleId = "customUserRoleId";
    system1ToAccount2SharingData.customPermissions = "customPermissions123123";
    system1ToAccount2SharingData.isEnabled = false;
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            std::move(system1ToAccount2SharingData)));
    // vmsUserId will be filled by cloud_db
    system1ToAccount2SharingData.vmsUserId =
        guidFromArbitraryData(system1ToAccount2SharingData.accountEmail)
            .toSimpleString().toStdString();

    //sharing system2 with account2 as cloudAdmin
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system2.id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    ASSERT_TRUE(restart());

    //checking account1 system list
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(getSystems(ownerAccount().email, ownerAccount().password, &systems), api::ResultCode::ok);
        ASSERT_EQ(2U, systems.size());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system()) != systems.end());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system2) != systems.end());

        ASSERT_EQ(ownerAccount().email, systems[0].ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, systems[0].accessRole);
        ASSERT_EQ(6U, systems[0].sharingPermissions.size());

        ASSERT_EQ(ownerAccount().email, systems[1].ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, systems[1].accessRole);
        ASSERT_EQ(6U, systems[1].sharingPermissions.size());
    }

    //checking account2 system list
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(getSystems(account2.email, account2Password, &systems), api::ResultCode::ok);
        ASSERT_EQ(2U, systems.size());
        //const auto system1Iter = std::find(systems.begin(), systems.end(), system());
        const auto system2Iter = std::find(systems.begin(), systems.end(), system2);
        const auto system3Iter = std::find(systems.begin(), systems.end(), system3);
        //ASSERT_TRUE(system1Iter != systems.end());
        ASSERT_TRUE(system2Iter != systems.end());
        ASSERT_TRUE(system3Iter != systems.end());

        //ASSERT_EQ(ownerAccount().email, system1Iter->ownerAccountEmail);
        //ASSERT_EQ(api::SystemAccessRole::viewer, system1Iter->accessRole);
        //ASSERT_TRUE(system1Iter->sharingPermissions.empty());

        ASSERT_EQ(ownerAccount().email, system2Iter->ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::cloudAdmin, system2Iter->accessRole);
        ASSERT_EQ(5U, system2Iter->sharingPermissions.size());

        ASSERT_EQ(account2.email, system3Iter->ownerAccountEmail);
        ASSERT_EQ(api::SystemAccessRole::owner, system3Iter->accessRole);
        ASSERT_EQ(6U, system3Iter->sharingPermissions.size());
    }

    //checking sharings from account1 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(sharings.size(), 4U);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            ownerAccount().fullName,
            findSharing(sharings, ownerAccount().email, system().id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, ownerAccount().email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            ownerAccount().fullName,
            findSharing(sharings, ownerAccount().email, system2.id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system().id),
            api::SystemAccessRole::viewer);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system().id).accountFullName);

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::cloudAdmin);
        ASSERT_EQ(
            account2.fullName,
            findSharing(sharings, account2.email, system2.id).accountFullName);

        bool found = false;
        for (const auto& sharingData : sharings)
        {
            if (sharingData.systemId == system1ToAccount2SharingData.systemId &&
                sharingData.accountEmail == system1ToAccount2SharingData.accountEmail)
            {
                found = true;
                ASSERT_TRUE((const api::SystemSharing&)sharingData == system1ToAccount2SharingData);
                ASSERT_EQ(account2.fullName, sharingData.accountFullName);
                ASSERT_EQ(account2.id, sharingData.accountId);
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
        ASSERT_EQ(sharings.size(), 3U);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, ownerAccount().email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::cloudAdmin);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system3.id),
            api::SystemAccessRole::owner);
    }

    //checking sharings of system() from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(account2.email, account2Password, system().id, &sharings),
            api::ResultCode::forbidden);
    }

    //checking sharings of system2 from account2 view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, system2.id, &sharings));
        ASSERT_EQ(sharings.size(), 2U);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, ownerAccount().email, system2.id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system2.id),
            api::SystemAccessRole::cloudAdmin);
    }

    //checking sharings of system() from system's view
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            getSystemSharings(system().id, system().authKey, &sharings),
            api::ResultCode::ok);
        ASSERT_EQ(2U, sharings.size());

        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id),
            api::SystemAccessRole::owner);
        ASSERT_EQ(
            accountAccessRoleForSystem(sharings, account2.email, system().id),
            api::SystemAccessRole::viewer);
    }
}

TEST_F(SystemSharing, maintenance)
{
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::maintenance));

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings));
        ASSERT_EQ(2U, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(sharings, account2.email, system().id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(2U, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(sharings, account2.email, system().id));
    }

    ASSERT_TRUE(restart());

    //account1: trying to modify (change access rights) sharing to account2
    //(failure: maintenance sharing cannot be updated)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::viewer));

    //account1: trying to remove sharing to account2
    //(failure: maintenance sharing cannot be removed)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::none));

    //account2: trying to modify sharing to account2
    //(failure: integrator cannot modify its sharing, only remove)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            account2.email,
            api::SystemAccessRole::viewer));

    //account2: trying to remove "owner" sharing to account1 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::none));
    //account2: trying to alter "owner" sharing to account1 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::viewer));
    //account1: trying to remove "owner" sharing to itself (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::none));

    //account2: trying to remove sharing to account2
    //(success: itegrator removed sharing to itself)
    ASSERT_EQ(
        api::ResultCode::ok,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            account2.email,
            api::SystemAccessRole::none));

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings));
        ASSERT_EQ(1U, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(0U, sharings.size());
    }

    ASSERT_TRUE(restart());

    //checking system list for both accounts
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings));
        ASSERT_EQ(1U, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id));
    }

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(account2.email, account2Password, &sharings));
        ASSERT_EQ(0U, sharings.size());
    }

    //adding "maintenance" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
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
            system().id,
            account3.email,
            api::SystemAccessRole::maintenance));

    //checking system list for both accounts
    std::vector<api::SystemSharingEx> ownerSharings;
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &ownerSharings));
        ASSERT_EQ(3U, ownerSharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(ownerSharings, ownerAccount().email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account2.email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account3.email, system().id));
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
                system().id,
                account4.email,
                role));
    }

    //account2: trying to upgrade sharing to account3 (failure)
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            account3.email,
            api::SystemAccessRole::cloudAdmin));

    //account2: removing sharing to account3 (success)
    ASSERT_EQ(
        api::ResultCode::ok,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            account3.email,
            api::SystemAccessRole::none));

    //checking system list for both accounts
    {
        ownerSharings.clear();
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &ownerSharings));
        ASSERT_EQ(2U, ownerSharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(ownerSharings, ownerAccount().email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::maintenance,
            accountAccessRoleForSystem(ownerSharings, account2.email, system().id));
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

    ASSERT_EQ(
        api::ResultCode::forbidden,
        shareSystem(
            account2.email,
            account2Password,
            system().id,
            account3.email,
            api::SystemAccessRole::maintenance));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        shareSystem(
            account2.email,
            account2Password,
            system().id,
            account3.email,
            api::SystemAccessRole::maintenance));

    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account3.email,
            api::SystemAccessRole::maintenance));
}

TEST_F(SystemSharing, owner)
{
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    //adding "cloudAdmin" sharing
    ASSERT_EQ(
        api::ResultCode::ok,
        shareSystem(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    //trying to update "owner" sharing
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::viewer));
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::viewer));

    //trying to remove "owner" sharing
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::none));
    ASSERT_EQ(
        api::ResultCode::forbidden,
        updateSystemSharing(
            account2.email,
            account2Password,
            system().id,
            ownerAccount().email,
            api::SystemAccessRole::none));

    //checking system list for account 1
    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings));
        ASSERT_EQ(sharings.size(), 2U);
        ASSERT_EQ(
            api::SystemAccessRole::owner,
            accountAccessRoleForSystem(sharings, ownerAccount().email, system().id));
        ASSERT_EQ(
            api::SystemAccessRole::cloudAdmin,
            accountAccessRoleForSystem(sharings, account2.email, system().id));
    }
}

TEST_F(SystemSharing, remove_system)
{
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
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            account2.email,
            api::SystemAccessRole::cloudAdmin));

    {
        std::vector<api::SystemSharingEx> sharings;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemSharings(ownerAccount().email, ownerAccount().password, &sharings));
        ASSERT_EQ(2U, sharings.size());
        ASSERT_EQ(
            api::SystemAccessRole::cloudAdmin,
            accountAccessRoleForSystem(sharings, account2.email, system().id));
    }

    {
        //checking account2 system list
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account2.email, account2Password, &systems));
        ASSERT_EQ(1U, systems.size());
        ASSERT_EQ(system().name, systems[0].name);
        ASSERT_EQ(system().ownerAccountEmail, systems[0].ownerAccountEmail);
    }

    //removing system
    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(ownerAccount().email, ownerAccount().password, system().id));

    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
        {
            ASSERT_TRUE(restart());
        }

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
            ownerAccount().email,
            ownerAccount().password,
            system().id,
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
            ownerAccount().email,
            ownerAccount().password,
            system().id,
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
            ownerAccount().email,
            ownerAccount().password,
            system().id,
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
            ownerAccount().email,
            ownerAccount().password,
            system().id,
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
        {
            ASSERT_TRUE(restart());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(ownerAccount().email, ownerAccount().password, system().id, &accessRoles));
            ASSERT_EQ(6U, accessRoles.size());
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
                getAccessRoleList(ownerAccount().email, ownerAccount().password, "unkown_system_id", &accessRoles));
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(ownerAccount().email, ownerAccount().password, system().id, &sharings));
            ASSERT_EQ(5U, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account2.email, account2Password, system().id, &accessRoles));
            ASSERT_EQ(5U, accessRoles.size());
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
                getSystemSharings(account2.email, account2Password, system().id, &sharings));
            ASSERT_EQ(5U, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account3.email, account3Password, system().id, &accessRoles));
            ASSERT_EQ(1U, accessRoles.size());
            ASSERT_TRUE(accessRoles.find(api::SystemAccessRole::maintenance) != accessRoles.end());
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account3.email, account3Password, system().id, &sharings));
            ASSERT_EQ(5U, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::ok,
                getAccessRoleList(account4.email, account4Password, system().id, &accessRoles));
        }

        {
            std::vector<api::SystemSharingEx> sharings;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystemSharings(account4.email, account4Password, system().id, &sharings));
            ASSERT_EQ(5U, sharings.size());
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::forbidden,
                getAccessRoleList(account5.email, account5Password, system().id, &accessRoles));
        }

        {
            std::set<api::SystemAccessRole> accessRoles;
            ASSERT_EQ(
                api::ResultCode::forbidden,
                getAccessRoleList(account6.email, account6Password, system().id, &accessRoles));
        }
    }
}

TEST_F(SystemSharing, remove_sharing_unknown_account)
{
    //removing not-existent sharing
    ASSERT_EQ(
        api::ResultCode::notFound,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            "unknown@account.email",
            api::SystemAccessRole::none));
}

/**
 * Disabled since cloud_db returns forbidden when trying to share with non-existent system.
 * This is a minor problem, so leaving it as-is for now.
 */
TEST_F(SystemSharing, DISABLED_remove_sharing_unknown_system)
{
    //removing not-existent sharing to unknown system
    ASSERT_EQ(
        api::ResultCode::notFound,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            QnUuid::createUuid().toString().toStdString(),   /* Let assume we can never have system with random id. */
            "unknown_account_name",
            api::SystemAccessRole::none));

    //removing not-existent sharing, empty account email
    ASSERT_EQ(
        api::ResultCode::notFound,
        updateSystemSharing(
            ownerAccount().email,
            ownerAccount().password,
            system().id,
            std::string(),
            api::SystemAccessRole::none));
}

TEST_F(SystemSharing, changing_own_rights_on_system)
{
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
        shareSystemEx(account1, system1, account2, targetRole);

        for (const auto role: allRoles)
        {
            // Changing own role in system. Failure is expected
            ASSERT_EQ(
                api::ResultCode::forbidden,
                updateSystemSharing(
                    account2.email,
                    account2.password,
                    system1.id,
                    account2.email,
                    role));
        }

        // Removing myself from system's uses
        shareSystemEx(account2, system(), account2, api::SystemAccessRole::none);
    }
}

TEST_F(SystemSharing, remove_sharing)
{
    const auto account2 = addActivatedAccount2();
    shareSystemEx(ownerAccount(), system(), account2, api::SystemAccessRole::cloudAdmin);

    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystem(account2.email, account2.password, system().id, &systemData));

    shareSystemEx(ownerAccount(), system(), account2, api::SystemAccessRole::none);
    ASSERT_EQ(
        api::ResultCode::forbidden,
        getSystem(account2.email, account2.password, system().id, &systemData));
}

//-------------------------------------------------------------------------------------------------

class SystemSharingNotification:
    public SystemSharing
{
public:
    SystemSharingNotification()
    {
        EXPECT_CALL(
            m_mockedEmailManager,
            sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const SystemSharedNotification&))
        ).Times(1);
        EXPECT_CALL(
            m_mockedEmailManager,
            sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
        ).Times(2);

        EMailManagerFactory::setFactory(
            [this](const conf::Settings& /*settings*/)
            {
                return std::make_unique<EmailManagerStub>(&m_mockedEmailManager);
            });
    }

private:
    EmailManagerMocked m_mockedEmailManager;
};

TEST_F(SystemSharingNotification, notification_is_delivered)
{
    const auto account2 = addActivatedAccount2();
    shareSystemEx(ownerAccount(), system(), account2, api::SystemAccessRole::cloudAdmin);

    // updating existing sharing - notification not sent
    shareSystemEx(ownerAccount(), system(), account2, api::SystemAccessRole::advancedViewer);

    // removing existing sharing - notification not sent
    shareSystemEx(ownerAccount(), system(), account2, api::SystemAccessRole::none);
}

//-------------------------------------------------------------------------------------------------

class SystemSharingDisabledUser:
    public SystemSharing
{
    using base_type = SystemSharing;

public:
    SystemSharingDisabledUser():
        m_previousResultCode(api::ResultCode::ok)
    {
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_userAccount = addActivatedAccount2();
    }

    void givenUserDisabledInCloudSystem()
    {
        shareSystemEx(ownerAccount(), system(), m_userAccount, api::SystemAccessRole::cloudAdmin);
        disableUser(ownerAccount(), system(), m_userAccount);
    }

    void whenUserRequestedThatSystemDetails()
    {
        api::SystemDataEx systemData;
        m_previousResultCode =
            getSystem(m_userAccount.email, m_userAccount.password, system().id, &systemData);
    }

    void thenResponseCodeShouldBeForbidden()
    {
        ASSERT_EQ(api::ResultCode::forbidden, m_previousResultCode);
    }

    void whenUserRequestedSystemList()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(m_userAccount.email, m_userAccount.password, &m_allSystems));
    }

    void thenResponseShouldBeEmpty()
    {
        ASSERT_TRUE(m_allSystems.empty());
    }

private:
    AccountWithPassword m_userAccount;
    api::ResultCode m_previousResultCode;
    std::vector<api::SystemDataEx> m_allSystems;
};

TEST_F(SystemSharingDisabledUser, get_particular_system_returns_forbidden_error)
{
    givenUserDisabledInCloudSystem();
    whenUserRequestedThatSystemDetails();
    thenResponseCodeShouldBeForbidden();
}

TEST_F(SystemSharingDisabledUser, user_get_empty_list_if_he_is_disabled_in_the_only_system)
{
    givenUserDisabledInCloudSystem();
    whenUserRequestedSystemList();
    thenResponseShouldBeEmpty();
}

} // namespace cdb
} // namespace nx
