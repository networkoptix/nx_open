#include "cloud_vms_synchro_test_helper.h"

#include <api/global_settings.h>
#include <nx_ec/data/api_fwd.h>
#include <nx_ec/ec_api.h>
#include <transaction/abstract_transaction_transport.h>
#include <utils/common/app_info.h>

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/network/app_info.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/test_support/test_options.h>

namespace nx::cloud::db {

static constexpr auto kMaxTimeToWaitForChangesToBePropagatedToCloud = std::chrono::minutes(1);
static constexpr auto kRetrySyncCheckPeriod = std::chrono::milliseconds(10);

Ec2MserverCloudSynchronization::Ec2MserverCloudSynchronization()
{
    const auto tmpDir =
        (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
            ? QDir::homePath()
            : nx::utils::TestOptions::temporaryDirectoryPath()) + "/ec2_cloud_sync_ut.data";
    QDir(tmpDir).removeRecursively();

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    m_appserver2.addArg(dbFileArg.toStdString().c_str());

    const QString p2pModeArg = lit("--p2pMode=%1").arg(GetParam().p2pMode ? 1 : 0);
    m_appserver2.addArg(p2pModeArg.toStdString().c_str());
}

Ec2MserverCloudSynchronization::~Ec2MserverCloudSynchronization()
{
    m_appserver2.stop();
    m_cdb.stop();
}

utils::test::ModuleLauncher<::ec2::Appserver2Process>*
    Ec2MserverCloudSynchronization::appserver2()
{
    return &m_appserver2;
}

CdbLauncher* Ec2MserverCloudSynchronization::cdb()
{
    return &m_cdb;
}

const CdbLauncher* Ec2MserverCloudSynchronization::cdb() const
{
    return &m_cdb;
}

api::ResultCode Ec2MserverCloudSynchronization::setOwnerAccountCredentials(
    const std::string& email,
    const std::string& password)
{
    const auto resultCode = m_cdb.getAccount(email, password, &m_account);
    if (resultCode != api::ResultCode::ok)
        return resultCode;
    m_account.password = password;
    return api::ResultCode::ok;
}

api::ResultCode Ec2MserverCloudSynchronization::bindSystemToOwnerAccount()
{
    // Adding system1 to account1.
    api::ResultCode result = m_cdb.bindRandomSystem(
        m_account.email, m_account.password, &m_system);
    if (result != api::ResultCode::ok)
        return result;

    // Saving cloud credentials to vms db.
    return saveCloudSystemCredentials(m_system.id, m_system.authKey);
}

api::ResultCode Ec2MserverCloudSynchronization::registerAccountAndBindSystemToIt()
{
    api::ResultCode result = api::ResultCode::ok;
    if (m_account.email.empty())    //< Account is not registered yet
    {
        result = m_cdb.addActivatedAccount(&m_account, &m_account.password);
        if (result != api::ResultCode::ok)
            return result;
    }

    // Adding system1 to account1.
    return bindSystemToOwnerAccount();
}

api::ResultCode Ec2MserverCloudSynchronization::unbindSystem()
{
    api::ResultCode result = m_cdb.unbindSystem(m_account.email, m_account.password, m_system.id);
    if (result != api::ResultCode::ok)
        return result;

    // Removing cloud users
    vms::api::UserDataList users;
    if (m_appserver2.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)
            ->getUsersSync(&users) != ::ec2::ErrorCode::ok)
    {
        return api::ResultCode::unknownError;
    }

    for (const auto& user: users)
    {
        if (!user.isCloud)
            continue;
        if (m_appserver2.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)
                ->removeSync(user.id) != ::ec2::ErrorCode::ok)
        {
            return api::ResultCode::unknownError;
        }
    }

    QnUuid adminUserId;
    if (!findAdminUserId(&adminUserId))
        return api::ResultCode::unknownError;

    auto settings =
        m_appserver2.moduleInstance()->ecConnection()->commonModule()->globalSettings();
    settings->setCloudSystemId(QString());
    settings->setCloudAuthKey(QString());
    settings->synchronizeNow();

    return api::ResultCode::ok;
}

api::ResultCode Ec2MserverCloudSynchronization::rebindSystem()
{
    auto resultCode = unbindSystem();
    if (resultCode != api::ResultCode::ok)
        return resultCode;

    return registerAccountAndBindSystemToIt();
}

api::ResultCode Ec2MserverCloudSynchronization::fillRegisteredSystemDataByCredentials(
    const std::string& systemId,
    const std::string& authKey)
{
    api::SystemDataEx system;
    const auto result = cdb()->getSystem(systemId, authKey, systemId, &system);
    if (result != api::ResultCode::ok)
        return result;
    m_system = system;
    m_system.authKey = authKey;
    return api::ResultCode::ok;
}

api::ResultCode Ec2MserverCloudSynchronization::saveCloudSystemCredentials(
    const std::string& /*id*/,
    const std::string& /*authKey*/)
{
    QnUuid adminUserId;
    if (!findAdminUserId(&adminUserId))
        return api::ResultCode::unknownError;

    auto settings = m_appserver2.moduleInstance()->ecConnection()->commonModule()->globalSettings();
    settings->setCloudSystemId(QString::fromStdString(m_system.id));
    settings->setCloudAuthKey(QString::fromStdString(m_system.authKey));
    settings->synchronizeNow();

    return api::ResultCode::ok;
}

const AccountWithPassword& Ec2MserverCloudSynchronization::ownerAccount() const
{
    return m_account;
}

const api::SystemData& Ec2MserverCloudSynchronization::registeredSystemData() const
{
    return m_system;
}

nx::utils::Url Ec2MserverCloudSynchronization::cdbEc2TransactionUrl() const
{
    nx::utils::Url url(lit("http://%1/").arg(cdb()->endpoint().toString()));
    url.setUserName(QString::fromStdString(m_system.id));
    url.setPassword(QString::fromStdString(m_system.authKey));
    return url;
}

void Ec2MserverCloudSynchronization::establishConnectionBetweenVmsAndCloud()
{
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(::ec2::kCloudPeerId, cdbEc2TransactionUrl());
}

void Ec2MserverCloudSynchronization::breakConnectionBetweenVmsAndCloud()
{
    appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(::ec2::kCloudPeerId);
}

void Ec2MserverCloudSynchronization::verifyTransactionConnection()
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

void Ec2MserverCloudSynchronization::testSynchronizingCloudOwner()
{
    api::SystemSharingEx sharingData;
    sharingData.accountEmail = ownerAccount().email;
    sharingData.accountFullName = ownerAccount().fullName;
    sharingData.isEnabled = true;
    sharingData.accessRole = api::SystemAccessRole::owner;

    constexpr auto testTime = std::chrono::minutes(1);
    waitForCloudUserToAppearInLocalDb(sharingData, testTime);
}

void Ec2MserverCloudSynchronization::testSynchronizingUserFromCloudToMediaServer()
{
    // Sharing system with some account.
    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&account2, &account2Password));

    api::SystemSharingEx sharingData;
    sharingData.systemId = registeredSystemData().id;
    sharingData.accountEmail = account2.email;
    sharingData.accessRole = api::SystemAccessRole::cloudAdmin;
    sharingData.userRoleId = "test_user_role";
    sharingData.accountFullName = account2.fullName;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->shareSystem(ownerAccount().email, ownerAccount().password, sharingData));

    // Waiting for new cloud user to arrive to local system.
    constexpr auto maxTimeToWait = std::chrono::minutes(1);
    waitForCloudUserToAppearInLocalDb(sharingData, maxTimeToWait);
}

void Ec2MserverCloudSynchronization::testSynchronizingUserFromMediaServerToCloud()
{
    // Adding cloud user.

    api::AccountData account3;
    std::string account3Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->addActivatedAccount(&account3, &account3Password));

    vms::api::UserData newCloudUser;
    addCloudUserLocally(account3.email, &newCloudUser);

    waitForUserToAppearInCloud(newCloudUser);
}

void Ec2MserverCloudSynchronization::addCloudUserLocally(
    const std::string& accountEmail,
    vms::api::UserData* const accountVmsData)
{
    // Adding cloud user locally.
    accountVmsData->id = guidFromArbitraryData(accountEmail);
    accountVmsData->typeId = QnUuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");
    accountVmsData->isCloud = true;
    accountVmsData->isEnabled = true;
    accountVmsData->email = QString::fromStdString(accountEmail);
    accountVmsData->name = QString::fromStdString(accountEmail);
    accountVmsData->userRoleId = QnUuid::createUuid();
    accountVmsData->realm = nx::network::AppInfo::realm();
    accountVmsData->hash = vms::api::UserData::kCloudPasswordStub;
    accountVmsData->digest = vms::api::UserData::kCloudPasswordStub;
    // TODO: randomize access rights
    accountVmsData->permissions = GlobalPermission::liveViewerPermissions;
    ASSERT_EQ(
        ::ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->saveSync(*accountVmsData));
}

void Ec2MserverCloudSynchronization::waitForUserToAppearInCloud(
    const vms::api::UserData& accountVmsData)
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
                ownerAccount().password,
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
                ASSERT_EQ(
                    accountVmsData.userRoleId.toSimpleString().toStdString(), user.userRoleId);
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

        std::this_thread::sleep_for(kRetrySyncCheckPeriod);
    }
}

void Ec2MserverCloudSynchronization::waitForUserToDisappearFromCloud(const std::string& email)
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
                ownerAccount().password,
                registeredSystemData().id,
                &systemUsers));

        if (cdb()->findSharing(systemUsers, email, registeredSystemData().id).accessRole
                == api::SystemAccessRole::none)
        {
            break;
        }

        std::this_thread::sleep_for(kRetrySyncCheckPeriod);
    }
}

void Ec2MserverCloudSynchronization::waitForUserToDisappearLocally(const QnUuid& userId)
{
    const auto t0 = std::chrono::steady_clock::now();
    for (;;)
    {
        ASSERT_LT(
            std::chrono::steady_clock::now(),
            t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

        vms::api::UserDataList users;
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

        const auto userIter = std::find_if(
            users.cbegin(), users.cend(),
            [userId](const vms::api::UserData& elem)
        {
            return elem.id == userId;
        });

        if (userIter == users.cend())
            break;

        std::this_thread::sleep_for(kRetrySyncCheckPeriod);
    }
}

void Ec2MserverCloudSynchronization::verifyCloudUserPresenceInLocalDb(
    const api::SystemSharingEx& sharingData,
    bool* const found,
    bool assertOnUserAbsense)
{
    if (found)
        *found = false;

    // Validating data.
    vms::api::UserDataList users;
    ASSERT_EQ(
        ::ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->getUsersSync(&users));

    const auto userIter = std::find_if(
        users.cbegin(), users.cend(),
        [email = sharingData.accountEmail](const vms::api::UserData& elem)
        {
            return elem.email.toStdString() == email;
        });
    if (assertOnUserAbsense)
    {
        ASSERT_NE(users.cend(), userIter);
    }

    if (userIter == users.cend())
        return;

    const vms::api::UserData& userData = *userIter;

    ASSERT_EQ(sharingData.accountEmail, userData.name.toStdString());
    //ASSERT_EQ(sharingData.accountFullName, userData.fullName.toStdString());
    ASSERT_TRUE(userData.isCloud);
    ASSERT_EQ(sharingData.isEnabled, userData.isEnabled);
    ASSERT_EQ("VMS", userData.realm);
    ASSERT_EQ(guidFromArbitraryData(sharingData.accountEmail), userData.id);
    ASSERT_FALSE(userData.isLdap);
    if (sharingData.accessRole == api::SystemAccessRole::owner)
    {
        ASSERT_TRUE(userData.isAdmin);
        ASSERT_EQ(GlobalPermissions(GlobalPermission::adminPermissions), userData.permissions);
    }

    // Verifying user full name.
    nx::vms::api::ResourceParamWithRefDataList kvPairs;
    ASSERT_EQ(
        ::ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getResourceManager(Qn::kSystemAccess)->getKvPairsSync(userData.id, &kvPairs));

    const auto fullNameIter = std::find_if(
        kvPairs.cbegin(), kvPairs.cend(),
        [](const nx::vms::api::ResourceParamWithRefData& element)
        {
            return element.name == Qn::USER_FULL_NAME;
        });

    if (assertOnUserAbsense)
    {
        ASSERT_TRUE(fullNameIter != kvPairs.cend());
        ASSERT_EQ(sharingData.accountFullName, fullNameIter->value.toStdString());
    }
    else
    {
        if (fullNameIter == kvPairs.cend())
            return;
        if (sharingData.accountFullName != fullNameIter->value.toStdString())
            return;
    }

    // TODO: #ak verify permissions
    if (found)
        *found = true;
}

void Ec2MserverCloudSynchronization::waitForCloudUserToAppearInLocalDb(
    const api::SystemSharingEx& sharingData,
    std::optional<std::chrono::milliseconds> maxTimeToWait)
{
    const auto t0 = std::chrono::steady_clock::now();
    // Waiting for new cloud user to arrive to local system.
    for (;;)
    {
        if (maxTimeToWait && (std::chrono::steady_clock::now() > (t0 + *maxTimeToWait)))
            break;

        bool found = false;
        verifyCloudUserPresenceInLocalDb(sharingData, &found, false);
        if (found)
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(false);
}

void Ec2MserverCloudSynchronization::fetchOwnerSharing(
    api::SystemSharingEx* const ownerSharing)
{
    std::vector<api::SystemSharingEx> systemUsers;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->getSystemSharings(
            ownerAccount().email,
            ownerAccount().password,
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

void Ec2MserverCloudSynchronization::waitUntilCloudAndVmsUsersMatch()
{
    for (;;)
    {
        bool usersMatch = false;
        verifyThatUsersMatchInCloudAndVms(false, &usersMatch);
        if (usersMatch)
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Ec2MserverCloudSynchronization::verifyThatUsersMatchInCloudAndVms(
    bool assertOnFailure,
    bool* const result)
{
    if (result)
        *result = false;

    // Selecting cloud users.
    std::vector<api::SystemSharingEx> cloudUsers;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->getSystemSharings(
            ownerAccount().email,
            ownerAccount().password,
            registeredSystemData().id,
            &cloudUsers));

    // Selecting local users.
    vms::api::UserDataList vmsUsers;
    ASSERT_EQ(
        ::ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getUserManager(Qn::kSystemAccess)->getUsersSync(&vmsUsers));

    vmsUsers.erase(
        vmsUsers.begin(),
        std::remove_if(vmsUsers.begin(), vmsUsers.end(),
            [](auto& user) { return user.isCloud; }));

    compareUsers(cloudUsers, vmsUsers, assertOnFailure, result);
}

void Ec2MserverCloudSynchronization::compareUsers(
    const std::vector<api::SystemSharingEx>& cloudUsers,
    const vms::api::UserDataList& vmsUsers,
    bool assertOnFailure,
    bool* result)
{
    if (assertOnFailure)
    {
        ASSERT_EQ(vmsUsers.size(), cloudUsers.size());
    }
    
    if (vmsUsers.size() != cloudUsers.size())
        return;

    for (const auto& cloudUser: cloudUsers)
    {
        bool found = false;
        verifyCloudUserPresenceInLocalDb(cloudUser, &found, assertOnFailure);
        if (!found)
            return;
    }

    if (result)
        *result = true;
}

void Ec2MserverCloudSynchronization::verifyThatSystemDataMatchInCloudAndVms(
    bool assertOnFailure,
    bool* const result)
{
    if (result)
        *result = false;

    // Fetching data from cloud
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->getSystem(
            ownerAccount().email,
            ownerAccount().password,
            registeredSystemData().id,
            &systemData));

    QnUuid adminUserId;
    ASSERT_TRUE(findAdminUserId(&adminUserId));

    nx::vms::api::ResourceParamWithRefDataList systemSettings;
    ASSERT_EQ(
        ::ec2::ErrorCode::ok,
        appserver2()->moduleInstance()->ecConnection()
            ->getResourceManager(Qn::kSystemAccess)->getKvPairsSync(adminUserId, &systemSettings));
    for (const auto systemSetting: systemSettings)
    {
        if (systemSetting.name == nx::settings_names::kNameSystemName)
        {
            if (assertOnFailure)
            {
                ASSERT_EQ(systemData.name, systemSetting.value.toStdString());
            }
            else if (systemData.name == systemSetting.value.toStdString())
            {
                if (result)
                    *result = true;
            }
            return;
        }
    }

    if (assertOnFailure)
    {
        ASSERT_TRUE(false) << nx::settings_names::kNameSystemName.toStdString()
            << " setting not found";
    }
}

void Ec2MserverCloudSynchronization::waitForCloudAndVmsToSyncUsers(
    bool assertOnFailure,
    bool* result)
{
    for (auto t0 = std::chrono::steady_clock::now();
        std::chrono::steady_clock::now() - t0 < kMaxTimeToWaitForChangesToBePropagatedToCloud;
        )
    {
        const bool isLastRun =
            (std::chrono::steady_clock::now() + std::chrono::seconds(1)) >=
            (t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

        bool syncCompleted = false;
        verifyThatUsersMatchInCloudAndVms(isLastRun, &syncCompleted);
        if (syncCompleted)
        {
            if (result)
                *result = true;
            return;
        }
        if (isLastRun)
            break;
        std::this_thread::sleep_for(kRetrySyncCheckPeriod);
    }

    if (assertOnFailure)
    {
        ASSERT_TRUE(false);
    }

    if (result)
        *result = false;
}

void Ec2MserverCloudSynchronization::waitForCloudAndVmsToSyncSystemData(
    bool assertOnFailure,
    bool* const result)
{
    for (auto t0 = std::chrono::steady_clock::now();
        std::chrono::steady_clock::now() - t0 < kMaxTimeToWaitForChangesToBePropagatedToCloud;
        )
    {
        const bool isLastRun =
            (std::chrono::steady_clock::now() + std::chrono::seconds(1)) >=
            (t0 + kMaxTimeToWaitForChangesToBePropagatedToCloud);

        bool syncCompleted = false;
        verifyThatSystemDataMatchInCloudAndVms(isLastRun, &syncCompleted);
        if (syncCompleted)
        {
            if (result)
                *result = true;
            return;
        }
        if (isLastRun)
            break;
        std::this_thread::sleep_for(kRetrySyncCheckPeriod);
    }

    if (assertOnFailure)
    {
        ASSERT_TRUE(false);
    }

    if (result)
        *result = false;
}

api::ResultCode Ec2MserverCloudSynchronization::fetchCloudTransactionLog(
    ::ec2::ApiTransactionDataList* const transactionList)
{
    const nx::utils::Url url(lm("http://%1%2?systemId=%3")
        .arg(cdb()->endpoint()).arg(kMaintenanceGetTransactionLog)
        .arg(registeredSystemData().id));
    return fetchTransactionLog(url, transactionList);
}

api::ResultCode Ec2MserverCloudSynchronization::fetchCloudTransactionLogFromMediaserver(
    ::ec2::ApiTransactionDataList* const transactionList)
{
    nx::utils::Url url(lm("http://%1/%2?cloud_only=true")
        .arg(appserver2()->moduleInstance()->endpoint()).arg("ec2/getTransactionLog"));
    url.setUserName("admin");
    url.setPassword("admin");
    return fetchTransactionLog(url, transactionList);
}

bool Ec2MserverCloudSynchronization::findAdminUserId(QnUuid* const id)
{
    vms::api::UserDataList users;
    if (m_appserver2.moduleInstance()->ecConnection()->getUserManager(Qn::kSystemAccess)
        ->getUsersSync(&users) != ::ec2::ErrorCode::ok)
    {
        return false;
    }

    QnUuid adminUserId;
    for (const auto& user : users)
    {
        if (user.name == "admin")
            adminUserId = user.id;
    }
    if (adminUserId.isNull())
        return false;

    *id = adminUserId;
    return true;
}

api::ResultCode Ec2MserverCloudSynchronization::fetchTransactionLog(
    const nx::utils::Url& url,
    ::ec2::ApiTransactionDataList* const transactionList)
{
    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
    httpClient.setMessageBodyReadTimeout(nx::network::kNoTimeout);
    if (!httpClient.doGet(url))
        return api::ResultCode::networkError;
    if (httpClient.response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
        return api::ResultCode::notAuthorized;

    nx::Buffer msgBody;
    while (!httpClient.eof())
        msgBody += httpClient.fetchMessageBodyBuffer();
    *transactionList = QJson::deserialized<::ec2::ApiTransactionDataList>(msgBody);

    return api::ResultCode::ok;
}

} // namespace nx::cloud::db
