#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/test_support/utils.h>

#include <api/global_settings.h>
#include <core/resource/user_resource.h>
#include <transaction/transaction_transport.h>
#include <utils/common/app_info.h>
#include <utils/common/id.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "email_manager_mocked.h"
#include "transaction_transport.h"

namespace nx {
namespace cdb {

TEST_F(Ec2MserverCloudSynchronization, general)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    for (int i = 0; i < 2; ++i)
    {
        // Cdb can change port after restart.
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
        if (i > 0)
            verifyThatUsersMatchInCloudAndVms();
        verifyTransactionConnection();
        testSynchronizingCloudOwner();
        testSynchronizingUserFromCloudToMediaServer();
        testSynchronizingUserFromMediaServerToCloud();

        if (i == 0)
            cdb()->restart();
    }
}

TEST_F(Ec2MserverCloudSynchronization, reconnecting)
{
    constexpr const int minDelay = 0;
    constexpr const int maxDelay = 100;

    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    for (int i = 0; i < 50; ++i)
    {
        // Cdb can change port after restart.
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                nx::utils::random::number(minDelay, maxDelay)));

        appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                nx::utils::random::number(minDelay, maxDelay)));
    }

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
    waitForCloudAndVmsToSyncUsers();
}

TEST_F(Ec2MserverCloudSynchronization, addingUserLocallyWhileOffline)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    // Sharing system with some account.
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&account2, &account2Password));

    for (int i = 0; i < 2; ++i)
    {
        cdb()->stop();

        // Adding user locally.
        ::ec2::ApiUserData account2VmsData;
        addCloudUserLocally(account2.email, &account2VmsData);

        ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
        verifyTransactionConnection();
        waitForUserToAppearInCloud(account2VmsData);
        // TODO: Check that cloud reported user's full name.

        // Checking that owner is in local DB.
        api::SystemSharingEx ownerSharing;
        fetchOwnerSharing(&ownerSharing);
        verifyCloudUserPresenceInLocalDb(ownerSharing);

        waitForCloudAndVmsToSyncUsers();

        if (i == 0)
        {
            // Removing user in cloud.
            api::SystemSharing sharingData;
            sharingData.accountEmail = account2VmsData.email.toStdString();
            sharingData.accessRole = api::SystemAccessRole::none;   //< Removing user.
            sharingData.systemID = registeredSystemData().id;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharingData));

            // Waiting for user to disappear from vms.
            waitForUserToDisappearLocally(account2VmsData.id);
        }
        else if (i == 1)
        {
            // Removing user locally.
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(account2VmsData.id));

            // Waiting for user to disappear in cloud.
            waitForUserToDisappearFromCloud(account2VmsData.email.toStdString());
        }
    }
}

TEST_F(Ec2MserverCloudSynchronization, mergingOfflineChanges)
{
    constexpr const int kTestAccountNumber = 10;

    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    // Adding multiple accounts.
    // vector<pair<account, password>>
    std::vector<std::pair<api::AccountData, std::string>> accounts(kTestAccountNumber);
    for (auto& account: accounts)
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->addActivatedAccount(&account.first, &account.second));
    }

    for (const auto& account: accounts)
    {
        // Adding user to vms.
        ::ec2::ApiUserData accountVmsData;
        addCloudUserLocally(account.first.email, &accountVmsData);

        // Adding users to the cloud.
        api::SystemSharing sharing;
        sharing.accountEmail = account.first.email;
        sharing.systemID = registeredSystemData().id;
        sharing.accessRole = static_cast<api::SystemAccessRole>(
            nx::utils::random::number<int>(
                static_cast<int>(api::SystemAccessRole::liveViewer),
                static_cast<int>(api::SystemAccessRole::maintenance)));

        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharing));
    }

    // Establishing connection.
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
    verifyTransactionConnection();

    // Waiting for data to be synchronized between cloud and vms.
    constexpr const auto kMaxSyncTime = std::chrono::seconds(10);
    for (const auto t0 = std::chrono::steady_clock::now();;)
    {
        ASSERT_LT(std::chrono::steady_clock::now(), t0 + kMaxSyncTime);

        bool usersMatch = false;
        verifyThatUsersMatchInCloudAndVms(false, &usersMatch);
        if (usersMatch)
            break;
    }
}

TEST_F(Ec2MserverCloudSynchronization, addingUserInCloudAndRemovingLocally)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    //for (int i = 0; i < 5; ++i)
    {
        ::ec2::ApiUserData accountVmsData;
        addCloudUserLocally(testAccount.email, &accountVmsData);

        ASSERT_EQ(
            ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(accountVmsData.id));

        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        api::SystemSharing sharing;
        sharing.accountEmail = testAccount.email;
        sharing.systemID = registeredSystemData().id;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharing));

        waitForCloudAndVmsToSyncUsers();

        ASSERT_EQ(
            ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(accountVmsData.id));

        waitForCloudAndVmsToSyncUsers();
    }

    ::ec2::ApiTransactionDataList transactionList;
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchCloudTransactionLog(&transactionList));
}

TEST_F(Ec2MserverCloudSynchronization, syncFromCloud)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    api::SystemSharing sharing;
    sharing.accountEmail = testAccount.email;
    sharing.systemID = registeredSystemData().id;
    sharing.accessRole = api::SystemAccessRole::cloudAdmin;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharing));

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    waitForCloudAndVmsToSyncUsers();
}

TEST_F(Ec2MserverCloudSynchronization, reBindingSystemToCloud)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    for (int i = 0; i < 2; ++i)
    {
        api::SystemSharing sharing;
        sharing.accountEmail = testAccount.email;
        sharing.systemID = registeredSystemData().id;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharing));

        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        if (i == 0)
        {
            appserver2()->moduleInstance()->ecConnection()
                ->deleteRemotePeer(cdbEc2TransactionUrl());
            ASSERT_EQ(api::ResultCode::ok, unbindSystem());
            cdb()->restart();
            ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());
        }
    }
}

TEST_F(Ec2MserverCloudSynchronization, newTransactionTimestamp)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    for (int i = 0; i < 2; ++i)
    {
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        ::ec2::ApiTransactionDataList transactionList;
        ASSERT_EQ(
            api::ResultCode::ok,
            fetchCloudTransactionLogFromMediaserver(&transactionList));
    
        for (const auto& transaction: transactionList)
        {
            ASSERT_EQ(
                registeredSystemData().systemSequence,
                transaction.tran.persistentInfo.timestamp.sequence);
        }

        if (i == 0)
        {
            appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());

            cdb()->restart();

            api::SystemSharing sharing;
            sharing.accountEmail = testAccount.email;
            sharing.systemID = registeredSystemData().id;
            sharing.accessRole = api::SystemAccessRole::cloudAdmin;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharing));
        }
    }
}

TEST_F(Ec2MserverCloudSynchronization, renameSystem)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    for (int i = 0; i < 4; ++i)
    {
        const bool needRestart = (i & 1) > 0;
        const bool updateInCloud = i < 2;

        if (needRestart)
        {
            appserver2()->moduleInstance()->ecConnection()
                ->deleteRemotePeer(cdbEc2TransactionUrl());
            cdb()->restart();
            appserver2()->moduleInstance()->ecConnection()
                ->addRemotePeer(cdbEc2TransactionUrl());
        }

        const std::string newSystemName =
            nx::utils::random::generate(10, 'a', 'z').toStdString();

        if (updateInCloud)
        {
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->renameSystem(
                    ownerAccount().email,
                    ownerAccountPassword(),
                    registeredSystemData().id,
                    newSystemName));
        }
        else
        {
            ::ec2::ApiResourceParamWithRefData param;
            param.resourceId = QnUserResource::kAdminGuid;
            param.name = QnGlobalSettings::kNameSystemName;
            param.value = QString::fromStdString(newSystemName);
            ::ec2::ApiResourceParamWithRefDataList paramList;
            paramList.push_back(std::move(param));

            ::ec2::ApiResourceParamWithRefDataList resultParamList;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                    ->getResourceManager(Qn::kSystemAccess)
                    ->saveSync(paramList, &resultParamList));
        }

        // Checking that system name has been updated in mediaserver.
        waitForCloudAndVmsToSyncSystemData();
    }
}

TEST_F(Ec2MserverCloudSynchronization, addingCloudUserWithNotRegisteredEmail)
{
    EmailManagerMocked mockedEmailManager;
#ifdef GMOCK_DYNAMIC_TYPE_MATCHER_PRESENT
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))).Times(1);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const InviteUserNotification&))).Times(1);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const RestorePasswordNotification&))).Times(1);
#else
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(::testing::_)).Times(3);
#endif
    //< One for owner account, another one - for user account, third one - password reset

    // Expecting send email call when adding unknown user.
    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/)
        {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    ::ec2::ApiUserData accountVmsData;
    const std::string testEmail = "test_123@mail.ru";
    addCloudUserLocally(testEmail, &accountVmsData);

    waitForCloudAndVmsToSyncUsers();

    // Checking that new account is actually there.
    std::string confirmationCode;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->resetAccountPassword(testEmail, &confirmationCode));
}

} // namespace cdb
} // namespace nx
