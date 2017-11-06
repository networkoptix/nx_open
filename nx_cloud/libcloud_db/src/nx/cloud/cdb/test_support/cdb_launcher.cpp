#include "cdb_launcher.h"

#include <chrono>
#include <functional>
#include <future>
#include <sstream>
#include <thread>
#include <tuple>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/app_info.h>

#include <nx/cloud/cdb/api/account_manager.h>
#include <nx/cloud/cdb/client/cdb_request_path.h>
#include <nx/cloud/cdb/client/data/types.h>

#include <transaction/transaction.h>

#include "business_data_generator.h"
#include "../cloud_db_service.h"
#include "../managers/email_manager.h"

namespace nx {
namespace cdb {

//-------------------------------------------------------------------------------------------------
// CdbLauncher

CdbLauncher::CdbLauncher(QString tmpDir):
    nx::utils::db::test::TestWithDbHelper("cdb", tmpDir),
    m_port(0),
    m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
    addArg("/path/to/bin");
    addArg("-e");
    addArg("-listenOn"); addArg(lit("127.0.0.1:0").toLatin1().constData());
    addArg("-log/logLevel"); addArg("DEBUG2");
    addArg("-dataDir"); addArg(testDataDir().toLatin1().constData());
    addArg("-syncroLog/logLevel"); addArg("DEBUG2");

    const auto dbConnectionOptionsToUse = dbConnectionOptions();

    addArg("-db/driverName");
    addArg(toString(dbConnectionOptionsToUse.driverType));

    if (!dbConnectionOptionsToUse.hostName.isEmpty())
    {
        addArg("-db/hostName");
        addArg(dbConnectionOptionsToUse.hostName.toUtf8().constData());
    }

    if (dbConnectionOptionsToUse.port != 0)
    {
        addArg("-db/port");
        addArg(QByteArray::number(dbConnectionOptionsToUse.port).constData());
    }

    addArg("-db/name");
    addArg(dbConnectionOptionsToUse.dbName.toUtf8().constData());

    if (!dbConnectionOptionsToUse.userName.isEmpty())
    {
        addArg("-db/userName");
        addArg(dbConnectionOptionsToUse.userName.toUtf8().constData());
    }

    if (!dbConnectionOptionsToUse.password.isEmpty())
    {
        addArg("-db/password");
        addArg(dbConnectionOptionsToUse.password.toUtf8().constData());
    }

    if (!dbConnectionOptionsToUse.connectOptions.isEmpty())
    {
        addArg("-db/connectOptions");
        addArg(dbConnectionOptionsToUse.connectOptions.toUtf8().constData());
    }

    addArg("-db/maxConnections");
    addArg(QByteArray::number(dbConnectionOptionsToUse.maxConnectionCount).constData());

    EMailManagerFactory::setFactory(
        [](const conf::Settings& /*settings*/){
            return std::make_unique<EmailManagerStub>(nullptr);
        });
}

CdbLauncher::~CdbLauncher()
{
    stop();
}

bool CdbLauncher::waitUntilStarted()
{
    constexpr const std::chrono::seconds kRequestTimeout = std::chrono::seconds(3);
    constexpr const int kRetryCount = 3;

    if (!utils::test::ModuleLauncher<CloudDbServicePublic>::waitUntilStarted())
        return false;

    const auto& httpEndpoints = moduleInstance()->impl()->httpEndpoints();
    if (httpEndpoints.empty())
        return false;
    m_port = httpEndpoints.front().port;

    m_connectionFactory->setCloudUrl(lit("http://127.0.0.1:%1").arg(m_port).toStdString());

    // Retrieving module info.
    for (int i = 0; i < kRetryCount; ++i)
    {
        auto connection = m_connectionFactory->createConnection();
        connection->setRequestTimeout(kRequestTimeout);
        api::ResultCode result = api::ResultCode::ok;
        std::tie(result, m_moduleInfo) = makeSyncCall<api::ResultCode, api::ModuleInfo>(
            std::bind(
                &nx::cdb::api::Connection::ping,
                connection.get(),
                std::placeholders::_1));
        if (result == api::ResultCode::ok)
            return true;
    }

    return false;
}

SocketAddress CdbLauncher::endpoint() const
{
    return SocketAddress(HostAddress::localhost, m_port);
}

nx::cdb::api::ConnectionFactory* CdbLauncher::connectionFactory()
{
    return m_connectionFactory.get();
}

std::unique_ptr<nx::cdb::api::Connection> CdbLauncher::connection(
    const std::string& login,
    const std::string& password)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(login, password);
    return connection;
}

api::ModuleInfo CdbLauncher::moduleInfo() const
{
    return m_moduleInfo;
}

api::ResultCode CdbLauncher::addAccount(
    api::AccountData* const accountData,
    std::string* const password,
    api::AccountConfirmationCode* const activationCode)
{
    if (accountData->email.empty())
        accountData->email = generateRandomEmailAddress();

    if (password->empty())
    {
        std::ostringstream ss;
        ss << nx::utils::random::number();
        *password = ss.str();
    }

    NX_ASSERT(!moduleInfo().realm.empty());

    if (accountData->fullName.empty())
        accountData->fullName = "Account " + accountData->email + " full name";
    if (accountData->passwordHa1.empty())
        accountData->passwordHa1 = nx_http::calcHa1(
            QUrl::fromPercentEncoding(QByteArray(accountData->email.c_str())).toLatin1().constData(),
            moduleInfo().realm.c_str(),
            password->c_str()).constData();
    if (accountData->passwordHa1Sha256.empty())
        accountData->passwordHa1Sha256 = nx_http::calcHa1(
            QUrl::fromPercentEncoding(QByteArray(accountData->email.c_str())).toLatin1().constData(),
            moduleInfo().realm.c_str(),
            password->c_str(),
            "SHA-256").constData();
    if (accountData->customization.empty())
        accountData->customization = nx::utils::AppInfo::customizationName().toStdString();

    auto connection = connectionFactory()->createConnection();

    api::AccountRegistrationData registrationData;
    registrationData.email = accountData->email;
    registrationData.passwordHa1 = accountData->passwordHa1;
    registrationData.passwordHa1Sha256 = accountData->passwordHa1Sha256;
    registrationData.fullName = accountData->fullName;
    registrationData.customization = accountData->customization;

    //adding account
    api::ResultCode result = api::ResultCode::ok;
    std::tie(result, *activationCode) = 
        makeSyncCall<api::ResultCode, api::AccountConfirmationCode>(
            std::bind(
                &nx::cdb::api::AccountManager::registerNewAccount,
                connection->accountManager(),
                registrationData,
                std::placeholders::_1));
    return result;
}

std::string CdbLauncher::generateRandomEmailAddress() const
{
    return test::BusinessDataGenerator::generateRandomEmailAddress();
}

api::ResultCode CdbLauncher::activateAccount(
    const api::AccountConfirmationCode& activationCode,
    std::string* const accountEmail)
{
    //activating account
    auto connection = connectionFactory()->createConnection();

    api::ResultCode result = api::ResultCode::ok;
    nx::cdb::api::AccountEmail response;
    std::tie(result, response) = makeSyncCall<api::ResultCode, nx::cdb::api::AccountEmail>(
        std::bind(
            &nx::cdb::api::AccountManager::activateAccount,
            connection->accountManager(),
            activationCode,
            std::placeholders::_1));
    *accountEmail = response.email;
    return result;
}

api::ResultCode CdbLauncher::reactivateAccount(
    const std::string& email,
    api::AccountConfirmationCode* const activationCode)
{
    auto connection = connectionFactory()->createConnection();

    api::ResultCode result = api::ResultCode::ok;
    api::AccountEmail accountEmail;
    accountEmail.email = email;
    std::tie(result, *activationCode) = makeSyncCall<api::ResultCode, nx::cdb::api::AccountConfirmationCode>(
        std::bind(
            &nx::cdb::api::AccountManager::reactivateAccount,
            connection->accountManager(),
            std::move(accountEmail),
            std::placeholders::_1));
    return result;
}

api::ResultCode CdbLauncher::getAccount(
    const std::string& email,
    const std::string& password,
    api::AccountData* const accountData)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *accountData) =
        makeSyncCall<api::ResultCode, nx::cdb::api::AccountData>(
            std::bind(
                &nx::cdb::api::AccountManager::getAccount,
                connection->accountManager(),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::addActivatedAccount(
    api::AccountData* const accountData,
    std::string* const password)
{
    api::AccountConfirmationCode activationCode;
    auto resCode = addAccount(
        accountData,
        password,
        &activationCode);
    if (resCode != api::ResultCode::ok)
        return resCode;

    if (activationCode.code.empty())
        return api::ResultCode::unknownError;

    std::string email;
    resCode = activateAccount(activationCode, &email);
    if (resCode != api::ResultCode::ok)
        return resCode;

    NX_ASSERT(accountData->email == email);

    resCode = getAccount(
        accountData->email,
        *password,
        accountData);
    return resCode;
}

api::ResultCode CdbLauncher::updateAccount(
    const std::string& email,
    const std::string& password,
    api::AccountUpdateData updateData)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::AccountManager::updateAccount,
                connection->accountManager(),
                std::move(updateData),
                std::placeholders::_1));

    return resCode;
}

api::ResultCode CdbLauncher::resetAccountPassword(
    const std::string& email,
    std::string* const confirmationCode)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, std::string());

    api::AccountEmail accountEmail;
    accountEmail.email = email;

    api::ResultCode resCode = api::ResultCode::ok;
    api::AccountConfirmationCode accountConfirmationCode;
    std::tie(resCode, accountConfirmationCode) =
        makeSyncCall<api::ResultCode, api::AccountConfirmationCode>(
            std::bind(
                &nx::cdb::api::AccountManager::resetPassword,
                connection->accountManager(),
                std::move(accountEmail),
                std::placeholders::_1));

    *confirmationCode = accountConfirmationCode.code;

    return resCode;
}

api::ResultCode CdbLauncher::createTemporaryCredentials(
    const std::string& email,
    const std::string& password,
    const api::TemporaryCredentialsParams& params,
    api::TemporaryCredentials* const temporaryCredentials)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resultCode = api::ResultCode::ok;
    std::tie(resultCode, *temporaryCredentials) =
        makeSyncCall<api::ResultCode, api::TemporaryCredentials>(
            std::bind(
                &nx::cdb::api::AccountManager::createTemporaryCredentials,
                connection->accountManager(),
                params,
                std::placeholders::_1));
    return resultCode;
}

api::ResultCode CdbLauncher::bindRandomNotActivatedSystem(
    const std::string& email,
    const std::string& password,
    api::SystemData* const systemData)
{
    return bindRandomNotActivatedSystem(email, password, std::string(), systemData);
}

api::ResultCode CdbLauncher::bindRandomNotActivatedSystem(
    const std::string& email,
    const std::string& password,
    const std::string& opaque,
    api::SystemData* const systemData)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::SystemRegistrationData sysRegData;
    std::ostringstream ss;
    ss << "test_sys_" << nx::utils::random::number();
    sysRegData.name = ss.str();
    sysRegData.opaque = opaque;
    if (!systemData->customization.empty())
        sysRegData.customization = systemData->customization;

    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode, *systemData) =
        makeSyncCall<api::ResultCode, api::SystemData>(
            std::bind(
                &nx::cdb::api::SystemManager::bindSystem,
                connection->systemManager(),
                std::move(sysRegData),
                std::placeholders::_1));

    return resCode;
}

api::ResultCode CdbLauncher::bindRandomSystem(
    const std::string& email,
    const std::string& password,
    api::SystemData* const systemData)
{
    return bindRandomSystem(email, password, std::string(), systemData);
}

api::ResultCode CdbLauncher::bindRandomSystem(
    const std::string& email,
    const std::string& password,
    const std::string& opaque,
    api::SystemData* const systemData)
{
    auto resCode = bindRandomNotActivatedSystem(email, password, opaque, systemData);
    if (resCode != api::ResultCode::ok)
        return resCode;

    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    //activating system
    api::NonceData nonceData;
    resCode = getCdbNonce(systemData->id, systemData->authKey, &nonceData);
    if (resCode != api::ResultCode::ok)
        return resCode;

    systemData->status = api::SystemStatus::ssActivated;
    return api::ResultCode::ok;
}

api::ResultCode CdbLauncher::unbindSystem(
    const std::string& login,
    const std::string& password,
    const std::string& systemId)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(login, password);

    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::unbindSystem,
                connection->systemManager(),
                systemId,
                std::placeholders::_1));

    return resCode;
}

api::ResultCode CdbLauncher::getSystems(
    const std::string& email,
    const std::string& password,
    std::vector<api::SystemDataEx>* const systems)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemDataExList systemDataList;
    std::tie(resCode, systemDataList) =
        makeSyncCall<api::ResultCode, api::SystemDataExList>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystems,
                connection->systemManager(),
                std::placeholders::_1));
    *systems = std::move(systemDataList.systems);

    return resCode;
}

api::ResultCode CdbLauncher::getSystemsFiltered(
    const std::string& email,
    const std::string& password,
    const api::Filter& filter,
    std::vector<api::SystemDataEx>* const systems)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemDataExList systemDataList;
    std::tie(resCode, systemDataList) =
        makeSyncCall<api::ResultCode, api::SystemDataExList>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystemsFiltered,
                connection->systemManager(),
                filter,
                std::placeholders::_1));
    *systems = std::move(systemDataList.systems);

    return resCode;
}

api::ResultCode CdbLauncher::getSystem(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    std::vector<api::SystemDataEx>* const systems)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemDataExList systemDataList;
    std::tie(resCode, systemDataList) =
        makeSyncCall<api::ResultCode, api::SystemDataExList>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystem,
                connection->systemManager(),
                systemId,
                std::placeholders::_1));
    *systems = std::move(systemDataList.systems);

    return resCode;
}

api::ResultCode CdbLauncher::getSystem(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    api::SystemDataEx* const system)
{
    std::vector<api::SystemDataEx> systems;
    const auto res = getSystem(email, password, systemId, &systems);
    if (res != api::ResultCode::ok)
        return res;

    if (systems.size() != 1)
        return api::ResultCode::unknownError;

    *system = systems[0];
    return api::ResultCode::ok;
}

api::ResultCode CdbLauncher::shareSystem(
    const std::string& email,
    const std::string& password,
    const api::SystemSharing& systemSharing)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::shareSystem,
                connection->systemManager(),
                std::move(systemSharing),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::shareSystem(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    const std::string& accountEmail,
    api::SystemAccessRole accessRole)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::SystemSharing systemSharing;
    systemSharing.accountEmail = accountEmail;
    systemSharing.systemId = systemId;
    systemSharing.accessRole = accessRole;

    return shareSystem(email, password, std::move(systemSharing));
}

api::ResultCode CdbLauncher::shareSystem(
    const AccountWithPassword& grantor,
    const std::string& systemId,
    const std::string& accountEmail,
    api::SystemAccessRole accessRole)
{
    return shareSystem(grantor.email, grantor.password, systemId, accountEmail, accessRole);
}

api::ResultCode CdbLauncher::updateSystemSharing(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    const std::string& accountEmail,
    api::SystemAccessRole newAccessRole)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::SystemSharing systemSharing;
    systemSharing.accountEmail = accountEmail;
    systemSharing.systemId = systemId;
    systemSharing.accessRole = newAccessRole;

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::shareSystem,
                connection->systemManager(),
                std::move(systemSharing),
                std::placeholders::_1));

    return resCode;
}

api::ResultCode CdbLauncher::removeSystemSharing(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    const std::string& accountEmail)
{
    return updateSystemSharing(
        email,
        password,
        systemId,
        accountEmail,
        api::SystemAccessRole::none);
}

api::ResultCode CdbLauncher::getSystemSharings(
    const std::string& email,
    const std::string& password,
    std::vector<api::SystemSharingEx>* const sharings)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    typedef void(nx::cdb::api::SystemManager::*GetCloudUsersOfSystemType)
        (std::function<void(api::ResultCode, api::SystemSharingExList)>);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemSharingExList data;
    std::tie(resCode, data) =
        makeSyncCall<api::ResultCode, api::SystemSharingExList>(
            std::bind(
                static_cast<GetCloudUsersOfSystemType>(
                    &nx::cdb::api::SystemManager::getCloudUsersOfSystem),
                connection->systemManager(),
                std::placeholders::_1));

    *sharings = std::move(data.sharing);
    return resCode;
}

api::ResultCode CdbLauncher::getAccessRoleList(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    std::set<api::SystemAccessRole>* const accessRoles)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemAccessRoleList accessRoleList;
    std::tie(resCode, accessRoleList) =
        makeSyncCall<api::ResultCode, api::SystemAccessRoleList>(
            std::bind(
                &nx::cdb::api::SystemManager::getAccessRoleList,
                connection->systemManager(),
                systemId,
                std::placeholders::_1));

    for (const auto& accessRoleData : accessRoleList.accessRoles)
        accessRoles->insert(accessRoleData.accessRole);
    return resCode;
}

api::ResultCode CdbLauncher::renameSystem(
    const std::string& login,
    const std::string& password,
    const std::string& systemId,
    const std::string& newSystemName)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(login, password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::rename,
                connection->systemManager(),
                systemId,
                newSystemName,
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::updateSystem(
    const api::SystemData& system,
    const api::SystemAttributesUpdate& updatedData)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(system.id, system.authKey);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::update,
                connection->systemManager(),
                updatedData,
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::getSystemSharings(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    std::vector<api::SystemSharingEx>* const sharings)
{
    typedef void(nx::cdb::api::SystemManager::*GetCloudUsersOfSystemType)
        (const std::string&, std::function<void(api::ResultCode, api::SystemSharingExList)>);

    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemSharingExList data;
    std::tie(resCode, data) =
        makeSyncCall<api::ResultCode, api::SystemSharingExList>(
            std::bind(
                static_cast<GetCloudUsersOfSystemType>(
                    &nx::cdb::api::SystemManager::getCloudUsersOfSystem),
                connection->systemManager(),
                systemId,
                std::placeholders::_1));

    *sharings = std::move(data.sharing);
    return resCode;
}

api::ResultCode CdbLauncher::getSystemSharing(
    const std::string& email,
    const std::string& password,
    const std::string& systemId,
    const std::string& userOfInterestEmail,
    api::SystemSharingEx* sharing)
{
    std::vector<api::SystemSharingEx> sharings;
    const auto resultCode = getSystemSharings(email, password, systemId, &sharings);
    if (resultCode != api::ResultCode::ok)
        return resultCode;
    const auto it = std::find_if(
        sharings.cbegin(), sharings.cend(),
        [&userOfInterestEmail](const api::SystemSharingEx& value)
        {
            return value.accountEmail == userOfInterestEmail;
        });

    if (it == sharings.cend())
        return api::ResultCode::notFound;

    *sharing = *it;
    return api::ResultCode::ok;
}

api::ResultCode CdbLauncher::getCdbNonce(
    const std::string& systemId,
    const std::string& authKey,
    api::NonceData* const nonceData)
{
    typedef void(nx::cdb::api::AuthProvider::*GetCdbNonceType)
        (std::function<void(api::ResultCode, api::NonceData)>);

    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(systemId, authKey);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *nonceData) =
        makeSyncCall<api::ResultCode, api::NonceData>(
            std::bind(
                static_cast<GetCdbNonceType>(&nx::cdb::api::AuthProvider::getCdbNonce),
                connection->authProvider(),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::getCdbNonce(
    const std::string& accountEmail,
    const std::string& accountPassword,
    const std::string& systemId,
    api::NonceData* const nonceData)
{
    typedef void(nx::cdb::api::AuthProvider::*GetCdbNonceType)
        (const std::string&, std::function<void(api::ResultCode, api::NonceData)>);

    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(accountEmail, accountPassword);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *nonceData) =
        makeSyncCall<api::ResultCode, api::NonceData>(
            std::bind(
                static_cast<GetCdbNonceType>(&nx::cdb::api::AuthProvider::getCdbNonce),
                connection->authProvider(),
                systemId,
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::ping(
    const std::string& systemId,
    const std::string& authKey)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(systemId, authKey);

    api::ResultCode resCode = api::ResultCode::ok;
    nx::cdb::api::ModuleInfo cloudModuleInfo;
    std::tie(resCode, cloudModuleInfo) =
        makeSyncCall<nx::cdb::api::ResultCode, nx::cdb::api::ModuleInfo>(
            std::bind(
                &nx::cdb::api::Connection::ping,
                connection.get(),
                std::placeholders::_1));
    return resCode;
}

const api::SystemSharingEx& CdbLauncher::findSharing(
    const std::vector<api::SystemSharingEx>& sharings,
    const std::string& accountEmail,
    const std::string& systemId) const
{
    static const api::SystemSharingEx kDummySharing;

    for (const auto& sharing : sharings)
    {
        if (sharing.accountEmail == accountEmail && sharing.systemId == systemId)
            return sharing;
    }

    return kDummySharing;
}

api::SystemAccessRole CdbLauncher::accountAccessRoleForSystem(
    const std::vector<api::SystemSharingEx>& sharings,
    const std::string& accountEmail,
    const std::string& systemId) const
{
    return findSharing(sharings, accountEmail, systemId).accessRole;
}

api::ResultCode CdbLauncher::fetchSystemData(
    const std::string& accountEmail,
    const std::string& accountPassword,
    const std::string& systemId,
    api::SystemDataEx* const systemData)
{
    std::vector<api::SystemDataEx> systems;
    const auto result = getSystems(accountEmail, accountPassword, &systems);
    if (result != api::ResultCode::ok)
        return result;
    for (auto& system : systems)
    {
        if (system.id == systemId)
        {
            *systemData = system;
            return api::ResultCode::ok;
        }
    }
    return api::ResultCode::notFound;
}

api::ResultCode CdbLauncher::getSystemHealthHistory(
    const std::string& accountEmail,
    const std::string& accountPassword,
    const std::string& systemId,
    api::SystemHealthHistory* history)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(accountEmail, accountPassword);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *history) =
        makeSyncCall<api::ResultCode, api::SystemHealthHistory>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystemHealthHistory,
                connection->systemManager(),
                systemId,
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::recordUserSessionStart(
    const AccountWithPassword& account,
    const std::string& systemId)
{
    auto connection = connectionFactory()->createConnection();
    connection->setCredentials(account.email, account.password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode) =
        makeSyncCall<nx::cdb::api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::recordUserSessionStart,
                connection->systemManager(),
                systemId,
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::getVmsConnections(
    api::VmsConnectionDataList* const vmsConnections)
{
    auto connection = connectionFactory()->createConnection();

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *vmsConnections) =
        makeSyncCall<nx::cdb::api::ResultCode, api::VmsConnectionDataList>(
            std::bind(
                &nx::cdb::api::MaintenanceManager::getConnectionsFromVms,
                connection->maintenanceManager(),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbLauncher::getTransactionLog(
    const std::string& accountEmail,
    const std::string& accountPassword,
    const std::string& systemId,
    ::ec2::ApiTransactionDataList* const transactions)
{
    const auto requestUrl = nx::network::url::Builder()
        .setScheme(nx_http::kUrlSchemeName).setEndpoint(endpoint())
        .setUserName(accountEmail.c_str()).setPassword(accountPassword.c_str())
        .setPath(kMaintenanceGetTransactionLog).setQuery(lm("systemId=%1").args(systemId));

    nx_http::HttpClient httpClient;
    if (!httpClient.doGet(requestUrl))
        return api::ResultCode::networkError;

    if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        return api::httpStatusCodeToResultCode(
            static_cast<nx_http::StatusCode::Value>(
                httpClient.response()->statusLine.statusCode));
    }

    nx::Buffer msgBody;
    while (!httpClient.eof())
        msgBody += httpClient.fetchMessageBodyBuffer();

    *transactions = QJson::deserialized<::ec2::ApiTransactionDataList>(msgBody);

    return api::ResultCode::ok;
}

api::ResultCode CdbLauncher::getStatistics(api::Statistics* const statistics)
{
    auto connection = connectionFactory()->createConnection();

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *statistics) =
        makeSyncCall<nx::cdb::api::ResultCode, api::Statistics>(
            std::bind(
                &nx::cdb::api::MaintenanceManager::getStatistics,
                connection->maintenanceManager(),
                std::placeholders::_1));
    return resCode;
}

bool CdbLauncher::isStartedWithExternalDb() const
{
    const nx::utils::db::ConnectionOptions connectionOptions = dbConnectionOptions();
    return !connectionOptions.dbName.isEmpty();
}

bool CdbLauncher::placePreparedDB(const QString& dbDumpPath)
{
    //starting with old db
    const nx::utils::db::ConnectionOptions connectionOptions = dbConnectionOptions();
    if (!connectionOptions.dbName.isEmpty())
        return false; //test is started with external DB: ignoring

    if (!QDir().mkpath(testDataDir()))
        return false;
    const QString dbPath = QDir::cleanPath(testDataDir() + "/cdb_ut.sqlite");
    QDir().remove(dbPath);
    if (!QFile::copy(dbDumpPath, dbPath))
        return false;
    return QFile(dbPath).setPermissions(
        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
        QFileDevice::ReadUser | QFileDevice::WriteUser);
}

AccountWithPassword CdbLauncher::addActivatedAccount2()
{
    //creating two accounts
    AccountWithPassword account;
    const auto result = CdbLauncher::addActivatedAccount(&account, &account.password);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to add account. %1")
            .arg(api::toString(result)).toStdString());
    }
    return account;
}

api::SystemData CdbLauncher::addRandomSystemToAccount(
    const AccountWithPassword& account)
{
    return addRandomSystemToAccount(account, api::SystemData());
}

api::SystemData CdbLauncher::addRandomSystemToAccount(
    const AccountWithPassword& account,
    const api::SystemData& systemPrototype)
{
    api::SystemData system1 = systemPrototype;
    const auto result = CdbLauncher::bindRandomSystem(account.email, account.password, &system1);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to bind system to account. %1")
            .arg(api::toString(result)).toStdString());
    }
    return system1;
}

void CdbLauncher::shareSystemEx(
    const AccountWithPassword& from,
    const api::SystemData& what,
    const AccountWithPassword& to,
    api::SystemAccessRole targetRole)
{
    const auto result = CdbLauncher::shareSystem(
        from.email, from.password, what.id, to.email, targetRole);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to share system. %1")
            .arg(api::toString(result)).toStdString());
    }
}

void CdbLauncher::shareSystemEx(
    const AccountWithPassword& from,
    const api::SystemData& what,
    const std::string& emailToShareWith,
    api::SystemAccessRole targetRole)
{
    auto result = CdbLauncher::shareSystem(
        from.email, from.password, what.id, emailToShareWith, targetRole);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to share system. %1")
            .arg(api::toString(result)).toStdString());
    }
}

void CdbLauncher::enableUser(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom)
{
    setUserEnabledFlag(who, what, whom, true);
}

void CdbLauncher::disableUser(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom)
{
    setUserEnabledFlag(who, what, whom, false);
}

void CdbLauncher::setUserEnabledFlag(
    const AccountWithPassword& who,
    const api::SystemData& what,
    const AccountWithPassword& whom,
    bool isEnabled)
{
    api::SystemSharingEx userData;
    auto result = getSystemSharing(who.email, who.password, what.id, whom.email, &userData);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to get system sharing. %1")
            .arg(api::toString(result)).toStdString());
    }

    userData.isEnabled = isEnabled;

    result = shareSystem(who.email, who.password, userData);
    if (result != api::ResultCode::ok)
    {
        throw std::runtime_error(lm("Failed to share system. %1")
            .arg(api::toString(result)).toStdString());
    }
}

namespace api {

bool operator==(const api::AccountData& left, const api::AccountData& right)
{
    return
        left.id == right.id &&
        left.email == right.email &&
        left.passwordHa1 == right.passwordHa1 &&
        left.fullName == right.fullName &&
        left.customization == right.customization &&
        left.statusCode == right.statusCode;
}

} // namespace api

//-------------------------------------------------------------------------------------------------
// EmailManagerStub

EmailManagerStub::EmailManagerStub(nx::cdb::AbstractEmailManager* const target):
    m_target(target)
{
}

void EmailManagerStub::sendAsync(
    const AbstractNotification& notification,
    std::function<void(bool)> completionHandler)
{
    if (!m_target)
        return;
    m_target->sendAsync(
        notification,
        std::move(completionHandler));
}

} // namespace cdb
} // namespace nx
