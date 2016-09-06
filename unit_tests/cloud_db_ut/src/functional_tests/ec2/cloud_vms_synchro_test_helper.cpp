#include "cloud_vms_synchro_test_helper.h"

#include <nx_ec/ec_api.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/thread/sync_queue.h>

#include <utils/common/app_info.h>

namespace nx {
namespace cdb {

constexpr static const auto kMaxTimeToWaitForChangesToBePropagatedToCloud = std::chrono::seconds(10);

Ec2MserverCloudSynchronization2::Ec2MserverCloudSynchronization2()
{
    const auto tmpDir =
        (CdbLauncher::temporaryDirectoryPath().isEmpty()
            ? QDir::homePath()
            : CdbLauncher::temporaryDirectoryPath()) + "/ec2_cloud_sync_ut.data";
    QDir(tmpDir).removeRecursively();

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    m_appserver2.addArg(dbFileArg.toStdString().c_str());
}

Ec2MserverCloudSynchronization2::~Ec2MserverCloudSynchronization2()
{
    m_appserver2.stop();
    m_cdb.stop();
}

utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>* 
    Ec2MserverCloudSynchronization2::appserver2()
{
    return &m_appserver2;
}

CdbLauncher* Ec2MserverCloudSynchronization2::cdb()
{
    return &m_cdb;
}

const CdbLauncher* Ec2MserverCloudSynchronization2::cdb() const
{
    return &m_cdb;
}

api::ResultCode Ec2MserverCloudSynchronization2::bindRandomSystem()
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
    for (const auto& user : users)
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

const api::AccountData& Ec2MserverCloudSynchronization2::ownerAccount() const
{
    return m_account;
}

const std::string& Ec2MserverCloudSynchronization2::ownerAccountPassword() const
{
    return m_accountPassword;
}

const api::SystemData& Ec2MserverCloudSynchronization2::registeredSystemData() const
{
    return m_system;
}

QUrl Ec2MserverCloudSynchronization2::cdbEc2TransactionUrl() const
{
    QUrl url(lit("http://%1/").arg(cdb()->endpoint().toString()));
    url.setUserName(QString::fromStdString(m_system.id));
    url.setPassword(QString::fromStdString(m_system.authKey));
    return url;
}

void Ec2MserverCloudSynchronization2::verifyTransactionConnection()
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

void Ec2MserverCloudSynchronization2::testSynchronizingCloudOwner()
{
    constexpr static const auto testTime = std::chrono::seconds(5);

    const auto deadline = std::chrono::steady_clock::now() + testTime;
    while (std::chrono::steady_clock::now() < deadline)
    {
        api::SystemSharingEx sharingData;
        sharingData.accountEmail = ownerAccount().email;
        sharingData.accountFullName = ownerAccount().fullName;
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

void Ec2MserverCloudSynchronization2::testSynchronizingUserFromCloudToMediaServer()
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
        cdb()->shareSystem(ownerAccount().email, ownerAccountPassword(), sharingData));

    // Waiting for new cloud user to arrive to local system.
    for (;;)
    {
        const auto newUser = newUsersQueue.pop(std::chrono::seconds(10));
        ASSERT_TRUE(static_cast<bool>(newUser));
        if (newUser->email.toStdString() == account2.email)
            break;
    }

    bool found = false;
    verifyCloudUserPresenceInLocalDb(sharingData, &found);
    ASSERT_TRUE(found);

    QObject::disconnect(connection);
}

void Ec2MserverCloudSynchronization2::testSynchronizingUserFromMediaServerToCloud()
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

void Ec2MserverCloudSynchronization2::addCloudUserLocally(
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

void Ec2MserverCloudSynchronization2::waitForUserToAppearInCloud(
    const ::ec2::ApiUserData& accountVmsData)
{
    // Waiting for it to appear in cloud.
    const auto t0 = std::chrono::steady_clock::now();
    for (;;)
    {
        ASSERT_LT(
            std::chrono::steady_clock::now(),
            t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

        std::vector<api::SystemSharingEx> systemUsers;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->getSystemSharings(
                ownerAccount().email,
                ownerAccountPassword(),
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

void Ec2MserverCloudSynchronization2::waitForUserToDisappearFromCloud(const std::string& email)
{
    // Waiting for it to appear in cloud.
    const auto t0 = std::chrono::steady_clock::now();
    for (;;)
    {
        ASSERT_LT(
            std::chrono::steady_clock::now(),
            t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

        std::vector<api::SystemSharingEx> systemUsers;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->getSystemSharings(
                ownerAccount().email,
                ownerAccountPassword(),
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

void Ec2MserverCloudSynchronization2::waitForUserToDisappearLocally(const QnUuid& userId)
{
    const auto t0 = std::chrono::steady_clock::now();
    for (;;)
    {
        ASSERT_LT(
            std::chrono::steady_clock::now(),
            t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

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

void Ec2MserverCloudSynchronization2::verifyCloudUserPresenceInLocalDb(
    const api::SystemSharingEx& sharingData,
    bool* const found,
    bool assertOnUserAbsense)
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

void Ec2MserverCloudSynchronization2::fetchOwnerSharing(api::SystemSharingEx* const ownerSharing)
{
    std::vector<api::SystemSharingEx> systemUsers;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->getSystemSharings(
            ownerAccount().email,
            ownerAccountPassword(),
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

void Ec2MserverCloudSynchronization2::verifyThatUsersMatchInCloudAndVms(
    bool assertOnFailure,
    bool* const result)
{
    if (result)
        *result = false;

    // Selecting local users.
    ec2::ApiUserDataList vmsUsers;
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->getUsersSync(&vmsUsers));

    // Selecting cloud users.
    std::vector<api::SystemSharingEx> sharings;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->getSystemSharings(
            ownerAccount().email,
            ownerAccountPassword(),
            registeredSystemData().id,
            &sharings));

    // Checking that number of cloud users in vms match that in cloud.
    const std::size_t numberOfCloudUsersInVms = std::count_if(
        vmsUsers.cbegin(), vmsUsers.cend(),
        [](const ec2::ApiUserData& data) { return data.isCloud; });
    if (assertOnFailure)
    {
        ASSERT_EQ(numberOfCloudUsersInVms, sharings.size());
    }
    else if (numberOfCloudUsersInVms != sharings.size())
    {
        return;
    }

    for (const auto& sharingData: sharings)
    {
        bool found = false;
        verifyCloudUserPresenceInLocalDb(sharingData, &found, assertOnFailure);
        if (!found)
            return;
    }

    if (result)
        *result = true;
}

} // namespace cdb
} // namespace nx
