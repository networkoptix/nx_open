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

TEST_F(Ec2MserverCloudSynchronization, establishConnection)
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
        constexpr static const auto kMaxTimeToWaitForConnection = std::chrono::seconds(5);

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
            ec2::ApiUserDataList users;
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                appserver2()->moduleInstance()->ecConnection()
                    ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

            auto cloudOwnerIter = std::find_if(
                users.begin(), users.end(),
                [email = account().email](const ec2::ApiUserData& elem)
                {
                    return elem.email.toStdString() == email;
                });
            if (cloudOwnerIter != users.end())
            {
                ASSERT_TRUE(cloudOwnerIter->isAdmin);
                ASSERT_EQ(account().email, cloudOwnerIter->name.toStdString());
                ASSERT_EQ(account().fullName, cloudOwnerIter->fullName.toStdString());
                ASSERT_TRUE(cloudOwnerIter->isCloud);
                ASSERT_TRUE(cloudOwnerIter->isEnabled);
                ASSERT_EQ("VMS", cloudOwnerIter->realm);
                ASSERT_EQ(guidFromArbitraryData(account().email), cloudOwnerIter->id);
                ASSERT_FALSE(cloudOwnerIter->isLdap);
                ASSERT_EQ(Qn::GlobalAdminPermissionSet, cloudOwnerIter->permissions);
                return;
            }
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

        api::SystemSharing sharingData;
        sharingData.systemID = registeredSystemData().id;
        sharingData.accountEmail = account2.email;
        sharingData.accessRole = api::SystemAccessRole::cloudAdmin;
        sharingData.groupID = "test_group";
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

        // Validating data.
        ec2::ApiUserDataList users;
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
                ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

        //ASSERT_EQ(3, users.size()); //local admin, cloud admin, new user
        ASSERT_NE(
            users.end(),
            std::find_if(
                users.begin(), users.end(),
                [email = account2.email](const ec2::ApiUserData& elem)
                {
                    return elem.email.toStdString() == email;
                }));

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

        // Adding cloud user locally.
        ::ec2::ApiUserData newCloudUser;
        newCloudUser.id = guidFromArbitraryData(account3.email);
        newCloudUser.isCloud = true;
        newCloudUser.isEnabled = true;
        newCloudUser.email = QString::fromStdString(account3.email);
        newCloudUser.name = newCloudUser.email;
        newCloudUser.groupId = QnUuid::createUuid();
        newCloudUser.fullName = QString::fromStdString(account3.fullName);
        newCloudUser.realm = QnAppInfo::realm();
        newCloudUser.hash = "password_is_in_cloud";
        newCloudUser.digest = "password_is_in_cloud";
        newCloudUser.permissions = Qn::GlobalLiveViewerPermissionSet;
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->saveSync(newCloudUser);

        // Waiting for it to appear in cloud.
        const auto t0 = std::chrono::steady_clock::now();
        constexpr static const auto maxTimeToWaitForUserToAppearInCloud = std::chrono::seconds(10);
        for (;;)
        {
            ASSERT_LT(
                std::chrono::steady_clock::now(),
                t0 + maxTimeToWaitForUserToAppearInCloud);

            std::vector<api::SystemSharingEx> systemUsers;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->getSystemSharings(
                    account().email,
                    accountPassword(),
                    registeredSystemData().id,
                    &systemUsers));

            bool found = false;
            for (const auto& user: systemUsers)
            {
                if (user.accountEmail == account3.email)
                {
                    // TODO: Validating data
                    ASSERT_EQ(newCloudUser.isEnabled, user.isEnabled);
                    ASSERT_EQ(api::SystemAccessRole::liveViewer, user.accessRole);
                    ASSERT_EQ(account3.fullName, user.accountFullName);
                    found = true;
                    break;
                }
            }

            if (found)
                break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
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

    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

    for (int i = 0; i < 2; ++i)
    {
        verifyTransactionConnection();
        testSynchronizingCloudOwner();
        testSynchronizingUserFromCloudToMediaServer();
        testSynchronizingUserFromMediaServerToCloud();

        if (i == 0)
            cdb()->restart();
    }
}

} // namespace cdb
} // namespace nx
