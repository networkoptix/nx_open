#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>

#include <api/global_settings.h>
#include <core/resource/user_resource.h>
#include <transaction/transaction_transport.h>
#include <utils/common/app_info.h>
#include <utils/common/id.h>
#include <transaction/abstract_transaction_transport.h>

#include <nx/cloud/cdb/test_support/transaction_connection_helper.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "email_manager_mocked.h"

namespace nx {
namespace cdb {

class FtEc2MserverCloudSynchronization:
    public Ec2MserverCloudSynchronization
{
public:
    FtEc2MserverCloudSynchronization()
    {
        init();
    }

    ~FtEc2MserverCloudSynchronization()
    {
    }

private:
    void init()
    {
        ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
        ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
        ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());
    }
};

TEST_P(FtEc2MserverCloudSynchronization, general)
{
    for (int i = 0; i < 2; ++i)
    {
        // Cdb can change port after restart.
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(
            ::ec2::kCloudPeerId,
            cdbEc2TransactionUrl());
        if (i > 0)
            verifyThatUsersMatchInCloudAndVms();
        verifyTransactionConnection();
        testSynchronizingCloudOwner();
        testSynchronizingUserFromCloudToMediaServer();
        testSynchronizingUserFromMediaServerToCloud();

        if (i == 0)
        {
            ASSERT_TRUE(cdb()->restart());
        }
    }
}

TEST_P(FtEc2MserverCloudSynchronization, reconnecting)
{
    constexpr const int minDelay = 0;
    constexpr const int maxDelay = 100;

    for (int i = 0; i < 50; ++i)
    {
        // Cdb can change port after restart.
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                nx::utils::random::number(minDelay, maxDelay)));

        appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(::ec2::kCloudPeerId);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                nx::utils::random::number(minDelay, maxDelay)));
    }

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
    waitForCloudAndVmsToSyncUsers();
}

TEST_P(FtEc2MserverCloudSynchronization, adding_user_locally_while_offline)
{
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
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
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
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->removeSystemSharing(
                    ownerAccount().email,
                    ownerAccount().password,
                    registeredSystemData().id,
                    account2VmsData.email.toStdString()));

            // Waiting for user to disappear from vms.
            waitForUserToDisappearLocally(account2VmsData.id);
        }
        else if (i == 1)
        {
            // Removing user locally.
            ASSERT_EQ(
                ::ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(account2VmsData.id));

            // Waiting for user to disappear in cloud.
            waitForUserToDisappearFromCloud(account2VmsData.email.toStdString());
        }
    }
}

TEST_P(FtEc2MserverCloudSynchronization, merging_offline_changes)
{
    constexpr const int kTestAccountNumber = 10;

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
        sharing.systemId = registeredSystemData().id;
        sharing.accessRole = static_cast<api::SystemAccessRole>(
            nx::utils::random::number<int>(
                static_cast<int>(api::SystemAccessRole::liveViewer),
                static_cast<int>(api::SystemAccessRole::maintenance)));

        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharing));
    }

    // Establishing connection.
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
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

TEST_P(FtEc2MserverCloudSynchronization, adding_user_in_cloud_and_removing_locally)
{
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
            ::ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(accountVmsData.id));

        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        api::SystemSharing sharing;
        sharing.accountEmail = testAccount.email;
        sharing.systemId = registeredSystemData().id;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharing));

        waitForCloudAndVmsToSyncUsers();

        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->removeSync(accountVmsData.id));

        waitForCloudAndVmsToSyncUsers();
    }

    ::ec2::ApiTransactionDataList transactionList;
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchCloudTransactionLog(&transactionList));
}

TEST_P(FtEc2MserverCloudSynchronization, sync_from_cloud)
{
    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    api::SystemSharing sharing;
    sharing.accountEmail = testAccount.email;
    sharing.systemId = registeredSystemData().id;
    sharing.accessRole = api::SystemAccessRole::cloudAdmin;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharing));

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

    waitForCloudAndVmsToSyncUsers();
}

TEST_P(FtEc2MserverCloudSynchronization, rebinding_system_to_cloud)
{
    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    for (int i = 0; i < 2; ++i)
    {
        api::SystemSharing sharing;
        sharing.accountEmail = testAccount.email;
        sharing.systemId = registeredSystemData().id;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharing));

        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        if (i == 0)
        {
            appserver2()->moduleInstance()->ecConnection()
                ->deleteRemotePeer(::ec2::kCloudPeerId);
            ASSERT_EQ(api::ResultCode::ok, unbindSystem());
            ASSERT_TRUE(cdb()->restart());
            ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());
        }
    }
}

TEST_P(FtEc2MserverCloudSynchronization, new_transaction_timestamp)
{
    api::AccountData testAccount;
    std::string testAccountPassword;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&testAccount, &testAccountPassword));

    for (int i = 0; i < 2; ++i)
    {
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

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
            appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(::ec2::kCloudPeerId);

            ASSERT_TRUE(cdb()->restart());

            api::SystemSharing sharing;
            sharing.accountEmail = testAccount.email;
            sharing.systemId = registeredSystemData().id;
            sharing.accessRole = api::SystemAccessRole::cloudAdmin;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharing));
        }
    }
}

TEST_P(FtEc2MserverCloudSynchronization, rename_system)
{
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

    for (int i = 0; i < 4; ++i)
    {
        const bool needRestart = (i & 1) > 0;
        const bool updateInCloud = i < 2;

        if (needRestart)
        {
            appserver2()->moduleInstance()->ecConnection()
                ->deleteRemotePeer(::ec2::kCloudPeerId);
            ASSERT_TRUE(cdb()->restart());
            appserver2()->moduleInstance()->ecConnection()
                ->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
        }

        const std::string newSystemName(nx::utils::generateRandomName(10).data());
        if (updateInCloud)
        {
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->renameSystem(
                    ownerAccount().email,
                    ownerAccount().password,
                    registeredSystemData().id,
                    newSystemName));
        }
        else
        {
            ::ec2::ApiResourceParamWithRefData param;
            param.resourceId = QnUserResource::kAdminGuid;
            param.name = nx::settings_names::kNameSystemName;
            param.value = QString::fromStdString(newSystemName);
            ::ec2::ApiResourceParamWithRefDataList paramList;
            paramList.push_back(std::move(param));

            ASSERT_EQ(
                ::ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                    ->getResourceManager(Qn::kSystemAccess)
                    ->saveSync(paramList));
        }

        // Checking that system name has been updated in mediaserver.
        waitForCloudAndVmsToSyncSystemData();
    }
}

TEST_P(Ec2MserverCloudSynchronization, adding_cloud_user_with_not_registered_email)
{
    EmailManagerMocked mockedEmailManager;
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(1);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const InviteUserNotification&))
    ).Times(1);
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const RestorePasswordNotification&))
    ).Times(1);
    //< One for owner account, another one - for user account, third one - password reset

    EMailManagerFactory::setFactory(
        [&mockedEmailManager](const conf::Settings& /*settings*/)
        {
            return std::make_unique<EmailManagerStub>(&mockedEmailManager);
        });

    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

    // Waiting for cloud owner to be synchronized to vms db.
    waitForCloudAndVmsToSyncUsers();

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

TEST_P(Ec2MserverCloudSynchronization, migrate_transactions)
{
    const std::string preRegisteredCloudAccountEmail =
        "akolesnikov@networkoptix.com";
    const std::string preRegisteredCloudAccountPassword = "123";

    const std::string preRegisteredCloudSystemId =
        "00e2af6f-cabf-4792-b3e9-6075e7fe13c6";
    const std::string preRegisteredCloudSystemAuthKey =
        "c22c770a-f1ff-4db3-9bb1-7c021874d83e";

    if (cdb()->isStartedWithExternalDb())
        return;

    ASSERT_TRUE(cdb()->placePreparedDB(":/cdb_with_transactions.sqlite"));
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());

    ASSERT_EQ(
        api::ResultCode::ok,
        setOwnerAccountCredentials(
            preRegisteredCloudAccountEmail,
            preRegisteredCloudAccountPassword));

    ASSERT_EQ(
        api::ResultCode::ok,
        fillRegisteredSystemDataByCredentials(
            preRegisteredCloudSystemId,
            preRegisteredCloudSystemAuthKey));
    ASSERT_EQ(
        api::ResultCode::ok,
        saveCloudSystemCredentials(
            preRegisteredCloudSystemId,
            preRegisteredCloudSystemAuthKey));

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());

    waitForCloudAndVmsToSyncUsers();
}

TEST_P(FtEc2MserverCloudSynchronization, transaction_timestamp)
{
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
    waitForCloudAndVmsToSyncUsers();

    using namespace std::chrono;

    for (int i = 0; i < 2; ++i)
    {
        // Shifting transaction timestamp.
        auto timestamp = appserver2()->moduleInstance()->ecConnection()->getTransactionLogTime();
        timestamp.ticks += duration_cast<milliseconds>(hours(24)).count();
        appserver2()->moduleInstance()->ecConnection()->setTransactionLogTime(timestamp);

        if (i == 1)
        {
            appserver2()->moduleInstance()->ecConnection()
                ->deleteRemotePeer(::ec2::kCloudPeerId);
            ASSERT_TRUE(cdb()->restart());
            appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
        }

        // Adding user locally with shifted timestamp.
        const std::string newAccountEmail = cdb()->generateRandomEmailAddress();
        ::ec2::ApiUserData userData;
        addCloudUserLocally(newAccountEmail, &userData);

        waitForCloudAndVmsToSyncUsers();

        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->removeSystemSharing(
                ownerAccount().email,
                ownerAccount().password,
                registeredSystemData().id,
                newAccountEmail));
        waitForCloudAndVmsToSyncUsers();

        // Checking that user has been removed.
        std::vector<api::SystemSharingEx> systemUsers;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->getSystemSharings(
                ownerAccount().email,
                ownerAccount().password,
                registeredSystemData().id,
                &systemUsers));
        ASSERT_EQ(1U, systemUsers.size());
    }
}

TEST_P(FtEc2MserverCloudSynchronization, user_fullname_modification_pushed_to_vms_from_cloud)
{
    establishConnectionBetweenVmsAndCloud();
    waitForCloudAndVmsToSyncUsers();

    api::AccountUpdateData update;
    update.fullName = ownerAccount().fullName + "new";
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->updateAccount(ownerAccount().email, ownerAccount().password, update));

    waitForCloudAndVmsToSyncUsers();
}

INSTANTIATE_TEST_CASE_P(P2pMode, FtEc2MserverCloudSynchronization,
    ::testing::Values(TestParams(false), TestParams(true)
));

} // namespace cdb
} // namespace nx
