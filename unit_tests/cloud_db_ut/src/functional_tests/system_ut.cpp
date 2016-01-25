/**********************************************************
* Oct 12, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, system_unbind)
{
    startAndWaitUntilStarted();

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

    //adding system2 to account1
    api::SystemData system0;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system0));

    for (int i = 0; i < 4; ++i)
    {
        //adding system1 to account1
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(account1.email, account1Password, &system1));

        //checking account1 system list
        {
            std::vector<api::SystemData> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 2);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system0) != systems.end());
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(account1.email, systems[1].ownerAccountEmail);
        }

        //sharing system1 with account2 as viewer
        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(
                account1.email,
                account1Password,
                system1.id,
                account2.email,
                api::SystemAccessRole::editorWithSharing));

        switch (i)
        {
            case 0:
                //unbinding with owner credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(account1.email, account1Password, system1.id.toStdString()));
                break;
            case 1:
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id.toStdString(), system1.authKey, system1.id.toStdString()));
                break;
            case 2:
                //unbinding with owner credentials
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(account2.email, account2Password, system1.id.toStdString()));
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id.toStdString(), system1.authKey, system1.id.toStdString()));
                continue;
            case 3:
                //unbinding with other system credentials
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(system0.id.toStdString(), system0.authKey, system1.id.toStdString()));
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id.toStdString(), system1.authKey, system1.id.toStdString()));
                continue;
        }

        //checking account1 system list
        {
            std::vector<api::SystemData> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system0) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
        }

        restart();
    }
}

TEST_F(CdbFunctionalTest, system_get)
{
    startAndWaitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    //adding system2 to account1
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    {
        std::vector<api::SystemData> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(account1.email, account1Password, system1.id.toStdString(), &systems));
        ASSERT_EQ(1, systems.size());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
    }

    {
        //requesting unknown system
        std::vector<api::SystemData> systems;
        ASSERT_EQ(
            api::ResultCode::notFound,
            getSystem(account1.email, account1Password, "unknown_system_id", &systems));
        ASSERT_TRUE(systems.empty());
    }
}

TEST_F(CdbFunctionalTest, system_activation)
{
    startAndWaitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    for (int i = 0; i < 1; ++i)
    {
        //adding system1 to account1
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(account1.email, account1Password, &system1));

        //checking account1 system list
        {
            std::vector<api::SystemData> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssNotActivated, systems[0].status);
        }

        if (i == 0)
        {
            api::NonceData nonceData;
            auto resultCode = getCdbNonce(
                system1.id.toStdString(),
                system1.authKey,
                &nonceData);
            ASSERT_EQ(api::ResultCode::ok, resultCode);
        }
        //else if (i == 1)
        //{
        //    //activating with ping
        //    api::NonceData nonceData;
        //    auto resultCode = ping(
        //        system1.id.toStdString(),
        //        system1.authKey);
        //    ASSERT_EQ(api::ResultCode::ok, resultCode);
        //}
        system1.status = api::SystemStatus::ssActivated;

        //checking account1 system list
        {
            std::vector<api::SystemData> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        restart();

        //checking account1 system list
        {
            std::vector<api::SystemData> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        ASSERT_EQ(
            api::ResultCode::ok,
            unbindSystem(account1.email, account1Password, system1.id.toStdString()));
    }
}

}   //cdb
}   //nx
