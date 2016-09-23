#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <transaction/transaction_transport.h>
#include <utils/common/app_info.h>
#include <utils/common/id.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "transaction_transport.h"

namespace nx {
namespace cdb {

class Ec2MserverCloudSynchronization
:
    public CdbFunctionalTest
{
public:
    Ec2MserverCloudSynchronization()
    :
        m_moduleGuid(QnUuid::createUuid()),
        m_runningInstanceGuid(QnUuid::createUuid()),
        m_transactionConnectionIdSequence(0)
    {
    }

    /**
     * @return new connection id
     */
    int openTransactionConnection(
        const std::string& systemId,
        const std::string& systemAuthKey)
    {
        auto transactionConnection =
            std::make_unique<test::TransactionTransport>(
                localPeer(),
                systemId,
                systemAuthKey);
        QObject::connect(
            transactionConnection.get(), &test::TransactionTransport::stateChanged,
            transactionConnection.get(),
            [transactionConnection = transactionConnection.get(), this](
                test::TransactionTransport::State state)
            {
                onTransactionConnectionStateChanged(transactionConnection, state);
            },
            Qt::DirectConnection);

        std::lock_guard<std::mutex> lk(m_mutex);
        auto transactionConnectionPtr = transactionConnection.get();
        const int connectionId = ++m_transactionConnectionIdSequence;
        m_connections.emplace(
            connectionId,
            std::move(transactionConnection));
        const QUrl url(lit("http://%1/ec2/events").arg(endpoint().toString()));
        transactionConnectionPtr->doOutgoingConnect(url);

        return connectionId;
    }

    bool openTransactionConnectionAndWaitForResult(
        const std::string& systemId,
        const std::string& systemAuthKey,
        std::chrono::milliseconds durationToWait)
    {
        const int connectionId = openTransactionConnection(systemId, systemAuthKey);

        // Waiting for connection state change.
        const auto waitUntil = std::chrono::steady_clock::now() + durationToWait;
        std::unique_lock<std::mutex> lk(m_mutex);
        for (;;)
        {
            auto connectionIter = m_connections.find(connectionId);
            if (connectionIter == m_connections.end())
                return false;   //< Connection has been removed.
            switch (connectionIter->second->getState())
            {
                case test::TransactionTransport::Connected:
                case test::TransactionTransport::NeedStartStreaming:
                case test::TransactionTransport::ReadyForStreaming:
                    return true;

                default:
                    if (m_condition.wait_until(lk, waitUntil) == std::cv_status::timeout)
                        return false;
            }
        }
    }

private:
    QnUuid m_moduleGuid;
    QnUuid m_runningInstanceGuid;
    std::map<int, std::unique_ptr<test::TransactionTransport>> m_connections;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<int> m_transactionConnectionIdSequence;

    ec2::ApiPeerData localPeer() const
    {
        return ec2::ApiPeerData(
            m_moduleGuid,
            m_runningInstanceGuid,
            Qn::PT_Server);
    }

    void onTransactionConnectionStateChanged(
        ec2::QnTransactionTransportBase* /*connection*/,
        ec2::QnTransactionTransportBase::State /*newState*/)
    {
        m_condition.notify_all();
    }
};

TEST_F(Ec2MserverCloudSynchronization, DISABLED_establishConnection)
{
    constexpr static const auto kWaitTimeout = std::chrono::seconds(5);

    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    // Adding system1 to account1.
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    // Creating transaction connection.
    ASSERT_TRUE(
        openTransactionConnectionAndWaitForResult(
            system1.id, system1.authKey, kWaitTimeout));
}

TEST_F(Ec2MserverCloudSynchronization2, general)
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

TEST_F(Ec2MserverCloudSynchronization2, reconnecting)
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

TEST_F(Ec2MserverCloudSynchronization2, addingUserLocallyWhileOffline)
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

TEST_F(Ec2MserverCloudSynchronization2, mergingOfflineChanges)
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

TEST_F(Ec2MserverCloudSynchronization2, addingUserInCloudAndRemovingLocally)
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

TEST_F(Ec2MserverCloudSynchronization2, syncFromCloud)
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

TEST_F(Ec2MserverCloudSynchronization2, reBindingSystemToCloud)
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
            appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());
            ASSERT_EQ(api::ResultCode::ok, unbindSystem());
            cdb()->restart();
            ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());
        }
    }
}

TEST_F(Ec2MserverCloudSynchronization2, newTransactionTimestamp)
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

TEST_F(Ec2MserverCloudSynchronization2, updateSystemNameInCloud)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    for (int i = 0; i < 2; ++i)
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->updateSystemName(
                ownerAccount().email,
                ownerAccountPassword(),
                registeredSystemData().id,
                "new_name"));

        // Checking that system name has been updated in mediaserver.
        waitForCloudAndVmsToSyncSystemData();

        if (i == 0)
            cdb()->restart();
    }
}

TEST_F(Ec2MserverCloudSynchronization2, updateSystemNameInVms)
{
    // TODO:
}

} // namespace cdb
} // namespace nx
