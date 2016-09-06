#include <atomic>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/thread/sync_queue.h>

#include <transaction/transaction_transport.h>
#include <utils/common/app_info.h>

#include "ec2/appserver2_process.h"
#include "test_setup.h"
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
        ec2::QnTransactionTransportBase* connection,
        ec2::QnTransactionTransportBase::State newState)
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

class Ec2MserverCloudSynchronization2
    :
    public ::testing::Test
{
public:
    Ec2MserverCloudSynchronization2()
    {
        const auto tmpDir = 
            (CdbLauncher::temporaryDirectoryPath().isEmpty()
             ? QDir::homePath()
             : CdbLauncher::temporaryDirectoryPath()) + "/ec2_cloud_sync_ut.data";
        QDir(tmpDir).removeRecursively();

        const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
        m_appserver2.addArg(dbFileArg.toStdString().c_str());
    }

    ~Ec2MserverCloudSynchronization2()
    {
        m_appserver2.stop();
        m_cdb.stop();
    }

    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>* appserver2()
    {
        return &m_appserver2;
    }

    CdbLauncher* cdb()
    {
        return &m_cdb;
    }

    const CdbLauncher* cdb() const
    {
        return &m_cdb;
    }

    api::ResultCode bindRandomSystem()
    {
        api::ResultCode result = m_cdb.addActivatedAccount(&m_account, &m_accountPassword);
        if (result != api::ResultCode::ok)
            return result;

        // Adding system1 to account1.
        result = m_cdb.bindRandomSystem(m_account.email, m_accountPassword, &m_system);
        if (result != api::ResultCode::ok)
            return result;

        // Saving cloud credentials to vms db.
        ec2::ApiUserDataList users;
        if (m_appserver2.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)
                ->getUsersSync(&users) != ec2::ErrorCode::ok)
        {
            return api::ResultCode::unknownError;
        }

        QnUuid adminUserId;
        for (const auto& user: users)
        {
            if (user.name == "admin")
                adminUserId = user.id;
        }
        if (adminUserId.isNull())
            return api::ResultCode::notFound;

        ec2::ApiResourceParamWithRefDataList params;
        params.emplace_back(ec2::ApiResourceParamWithRefData(
            adminUserId,
            "cloudSystemID",
            QString::fromStdString(m_system.id)));
        params.emplace_back(ec2::ApiResourceParamWithRefData(
            adminUserId,
            "cloudAuthKey",
            QString::fromStdString(m_system.authKey)));
        ec2::ApiResourceParamWithRefDataList outParams;
        if (m_appserver2.moduleInstance()->ecConnection()->getResourceManager(Qn::kSystemAccess)
                ->saveSync(params, &outParams) != ec2::ErrorCode::ok)
        {
            return api::ResultCode::unknownError;
        }

        return api::ResultCode::ok;
    }

    const api::AccountData& account() const
    {
        return m_account;
    }

    const std::string& accountPassword() const
    {
        return m_accountPassword;
    }

    const api::SystemData& registeredSystemData() const
    {
        return m_system;
    }

    QUrl cdbEc2TransactionUrl() const
    {
        QUrl url(lit("http://%1/").arg(cdb()->endpoint().toString()));
        url.setUserName(QString::fromStdString(m_system.id));
        url.setPassword(QString::fromStdString(m_system.authKey));
        return url;
    }

protected:
    void verifyTransactionConnection()
    {
        constexpr static const auto kMaxTimeToWaitForConnection = std::chrono::seconds(15);

        for (const auto t0 = std::chrono::steady_clock::now();
             std::chrono::steady_clock::now() < (t0 + kMaxTimeToWaitForConnection);
             )
        {
            // Checking connection is there.
            api::VmsConnectionDataList vmsConnections;
            ASSERT_EQ(api::ResultCode::ok, cdb()->getVmsConnections(&vmsConnections));
            const auto connectionIt = std::find_if(
                vmsConnections.connections.cbegin(),
                vmsConnections.connections.cend(),
                [systemId = registeredSystemData().id](const api::VmsConnectionData& data)
                {
                    return data.systemId == systemId;
                });
            if (connectionIt != vmsConnections.connections.cend())
                return; //< Connection has been found

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        ASSERT_TRUE(false);
    }

    void testSynchronizingCloudOwner()
    {
        constexpr static const auto testTime = std::chrono::seconds(5);

        const auto deadline = std::chrono::steady_clock::now() + testTime;
        while (std::chrono::steady_clock::now() < deadline)
        {
            api::SystemSharingEx sharingData;
            sharingData.accountEmail = account().email;
            sharingData.accountFullName = account().fullName;
            sharingData.isEnabled = true;
            sharingData.accessRole = api::SystemAccessRole::owner;
            bool found = false;
            verifyCloudUserPresenceInLocalDb(sharingData, &found, false);
            if (found)
                return;
        }

        // Cloud owner has not arrived to local system.
        ASSERT_TRUE(false);
    }

    void testSynchronizingUserFromCloudToMediaServer()
    {
        nx::utils::SyncQueue<::ec2::ApiUserData> newUsersQueue;
        const auto connection = QObject::connect(
            appserver2()->moduleInstance()->ecConnection()->getUserNotificationManager().get(),
            &ec2::AbstractUserNotificationManager::addedOrUpdated,
            [&newUsersQueue](const ::ec2::ApiUserData& user)
            {
                newUsersQueue.push(user);
            });

        // Sharing system with some account.
        api::AccountData account2;
        std::string account2Password;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->addActivatedAccount(&account2, &account2Password));

        api::SystemSharingEx sharingData;
        sharingData.systemID = registeredSystemData().id;
        sharingData.accountEmail = account2.email;
        sharingData.accessRole = api::SystemAccessRole::cloudAdmin;
        sharingData.groupID = "test_group";
        sharingData.accountFullName = account2.fullName;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->shareSystem(account().email, accountPassword(), sharingData));

        // Waiting for new cloud user to arrive to local system.
        for (;;)
        {
            const auto newUser = newUsersQueue.pop(std::chrono::seconds(10));
            ASSERT_TRUE(newUser);
            if (newUser->email.toStdString() == account2.email)
                break;
        }

        bool found = false;
        verifyCloudUserPresenceInLocalDb(sharingData, &found);
        ASSERT_TRUE(found);

        QObject::disconnect(connection);
    }

    void testSynchronizingUserFromMediaServerToCloud()
    {
        // Adding cloud user.

        api::AccountData account3;
        std::string account3Password;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->addActivatedAccount(&account3, &account3Password));

        ::ec2::ApiUserData newCloudUser;
        addCloudUserLocally(account3.email, &newCloudUser);

        waitForUserToAppearInCloud(newCloudUser);
    }

    void addCloudUserLocally(
        const std::string& accountEmail,
        ::ec2::ApiUserData* const accountVmsData)
    {
        // Adding cloud user locally.
        accountVmsData->id = guidFromArbitraryData(accountEmail);
        accountVmsData->isCloud = true;
        accountVmsData->isEnabled = true;
        accountVmsData->email = QString::fromStdString(accountEmail);
        accountVmsData->name = QString::fromStdString(accountEmail);
        accountVmsData->groupId = QnUuid::createUuid();
        accountVmsData->realm = QnAppInfo::realm();
        accountVmsData->hash = "password_is_in_cloud";
        accountVmsData->digest = "password_is_in_cloud";
        // TODO: randomize access rights
        accountVmsData->permissions = Qn::GlobalLiveViewerPermissionSet;
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->saveSync(*accountVmsData));
    }

    constexpr static const auto maxTimeToWaitForChangesToBePropagatedToCloud = std::chrono::seconds(10);

    void waitForUserToAppearInCloud(
        const ::ec2::ApiUserData& accountVmsData)
    {
        // Waiting for it to appear in cloud.
        const auto t0 = std::chrono::steady_clock::now();
        for (;;)
        {
            ASSERT_LT(
                std::chrono::steady_clock::now(),
                t0 + maxTimeToWaitForChangesToBePropagatedToCloud);

            std::vector<api::SystemSharingEx> systemUsers;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->getSystemSharings(
                    account().email,
                    accountPassword(),
                    registeredSystemData().id,
                    &systemUsers));

            bool found = false;
            for (const auto& user : systemUsers)
            {
                if (user.accountEmail == accountVmsData.email.toStdString())
                {
                    // TODO: Validating data
                    ASSERT_EQ(accountVmsData.isEnabled, user.isEnabled);
                    ASSERT_EQ(accountVmsData.id.toSimpleString().toStdString(), user.vmsUserId);
                    ASSERT_EQ(accountVmsData.groupId.toSimpleString().toStdString(), user.groupID);
                    //ASSERT_EQ(api::SystemAccessRole::liveViewer, user.accessRole);
                    //ASSERT_EQ(accountVmsData.fullName.toStdString(), user.accountFullName);
                    ASSERT_EQ(
                        QnLexical::serialized(accountVmsData.permissions).toStdString(),
                        user.customPermissions);
                    found = true;
                    break;
                }
            }

            if (found)
                break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void waitForUserToDisappearFromCloud(const std::string& email)
    {
        // Waiting for it to appear in cloud.
        const auto t0 = std::chrono::steady_clock::now();
        for (;;)
        {
            ASSERT_LT(
                std::chrono::steady_clock::now(),
                t0 + maxTimeToWaitForChangesToBePropagatedToCloud);

            std::vector<api::SystemSharingEx> systemUsers;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->getSystemSharings(
                    account().email,
                    accountPassword(),
                    registeredSystemData().id,
                    &systemUsers));

            if (cdb()->findSharing(systemUsers, email, registeredSystemData().id).accessRole
                == api::SystemAccessRole::none)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void waitForUserToDisappearLocally(const QnUuid& userId)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (;;)
        {
            ASSERT_LT(
                std::chrono::steady_clock::now(),
                t0 + maxTimeToWaitForChangesToBePropagatedToCloud);

            ec2::ApiUserDataList users;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                    ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

            const auto userIter = std::find_if(
                users.cbegin(), users.cend(),
                [userId](const ec2::ApiUserData& elem)
                {
                    return elem.id == userId;
                });

            if (userIter == users.cend())
                break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    /**
     * @param found[out] Set to \a true if user has been found and verified. \a false otherwise
     */
    void verifyCloudUserPresenceInLocalDb(
        const api::SystemSharingEx& sharingData,
        bool* const found = nullptr,
        bool assertOnUserAbsense = true)
    {
        // Validating data.
        ec2::ApiUserDataList users;
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

        const auto userIter = std::find_if(
            users.cbegin(), users.cend(),
            [email = sharingData.accountEmail](const ec2::ApiUserData& elem)
            {
                return elem.email.toStdString() == email;
            });
        if (assertOnUserAbsense)
        {
            ASSERT_NE(users.cend(), userIter);
        }

        if (userIter == users.cend())
        {
            if (found)
                *found = false;
        }

        ASSERT_EQ(sharingData.accountEmail, userIter->name.toStdString());
        ASSERT_EQ(sharingData.accountFullName, userIter->fullName.toStdString());
        ASSERT_TRUE(userIter->isCloud);
        ASSERT_EQ(sharingData.isEnabled, userIter->isEnabled);
        ASSERT_EQ("VMS", userIter->realm);
        ASSERT_EQ(guidFromArbitraryData(sharingData.accountEmail), userIter->id);
        ASSERT_FALSE(userIter->isLdap);
        if (sharingData.accessRole == api::SystemAccessRole::owner)
        {
            ASSERT_TRUE(userIter->isAdmin);
            ASSERT_EQ(Qn::GlobalAdminPermissionSet, userIter->permissions);
        }

        // TODO: #ak verify permissions
        if (found)
            *found = true;
    }

    void fetchOwnerSharing(api::SystemSharingEx* const ownerSharing)
    {
        std::vector<api::SystemSharingEx> systemUsers;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->getSystemSharings(
                account().email,
                accountPassword(),
                registeredSystemData().id,
                &systemUsers));
        for (auto& sharingData: systemUsers)
        {
            if (sharingData.accessRole == api::SystemAccessRole::owner)
            {
                *ownerSharing = std::move(sharingData);
                return;
            }
        }

        ASSERT_TRUE(false);
    }

private:
    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> m_appserver2;
    CdbLauncher m_cdb;
    api::AccountData m_account;
    std::string m_accountPassword;
    api::SystemData m_system;
};

TEST_F(Ec2MserverCloudSynchronization2, general)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, bindRandomSystem());

    for (int i = 0; i < 2; ++i)
    {
        // Cdb can change port after restart.
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
        verifyTransactionConnection();
        testSynchronizingCloudOwner();
        testSynchronizingUserFromCloudToMediaServer();
        testSynchronizingUserFromMediaServerToCloud();

        if (i == 0)
            cdb()->restart();
    }
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

    cdb()->stop();
    
    // Adding user locally
    ::ec2::ApiUserData account2VmsData;
    addCloudUserLocally(account2.email, &account2VmsData);

    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
    verifyTransactionConnection();
    waitForUserToAppearInCloud(account2VmsData);
    // TODO: Check that cloud reported user's full name

    // Checking that owner is in local DB
    api::SystemSharingEx ownerSharing;
    fetchOwnerSharing(&ownerSharing);
    verifyCloudUserPresenceInLocalDb(ownerSharing);

#if 0
    // Removing user locally
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->removeSync(account2VmsData.id));

    // Waiting for user to disappear in cloud
    waitForUserToDisappearFromCloud(account2VmsData.email.toStdString());
#endif

    // Removing user in cloud
    api::SystemSharing sharingData;
    sharingData.accountEmail = account2VmsData.email.toStdString();
    sharingData.accessRole = api::SystemAccessRole::none;   //< Removing user
    sharingData.systemID = registeredSystemData().id;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->shareSystem(account().email, accountPassword(), sharingData));

    // Waiting for user to disappear from vms
    waitForUserToDisappearLocally(account2VmsData.id);
}

} // namespace cdb
} // namespace nx
