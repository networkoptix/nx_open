/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#include "test_setup.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <sstream>
#include <thread>
#include <tuple>

#include <cdb/account_manager.h>
#include <utils/common/cpp14.h>
#include <utils/common/sync_call.h>
#include <nx/network/http/auth_tools.h>

#include <libcloud_db/src/managers/email_manager.h>

#include "version.h"
#include "email_manager_mocked.h"


namespace nx {
namespace cdb {

CdbFunctionalTest::CdbFunctionalTest()
:
    m_port(0),
    m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
    m_port = (std::rand() % 10000) + 50000;
    m_tmpDir = QDir::homePath() + "/cdb_ut.data";
    QDir(m_tmpDir).removeRecursively();

    auto b = std::back_inserter(m_args);
    *b = strdup("/path/to/bin");
    *b = strdup("-e");
    *b = strdup("-listenOn"); *b = strdup(lit("127.0.0.1:%1").arg(m_port).toLatin1().constData());
    *b = strdup("-log/logLevel"); *b = strdup("DEBUG2");
    *b = strdup("-dataDir"); *b = strdup(m_tmpDir.toLatin1().constData());
    *b = strdup("-db/driverName"); *b = strdup("QSQLITE");
    *b = strdup("-db/name"); *b = strdup(lit("%1/%2").arg(m_tmpDir).arg(lit("cdb_ut.sqlite")).toLatin1().constData());

    m_connectionFactory->setCloudEndpoint("127.0.0.1", m_port);

    EMailManagerFactory::setFactory(
        [](const conf::Settings& /*settings*/){
            return std::make_unique<EmailManagerStub>(nullptr);
        });
}

CdbFunctionalTest::~CdbFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();
}

void CdbFunctionalTest::start()
{
    std::promise<void> cdbInstantiatedCreatedPromise;
    auto cdbInstantiatedCreatedFuture = cdbInstantiatedCreatedPromise.get_future();

    m_cdbProcessFuture = std::async(
        std::launch::async,
        [this, &cdbInstantiatedCreatedPromise]()->int {
            m_cdbInstance = std::make_unique<nx::cdb::CloudDBProcessPublic>(
                static_cast<int>(m_args.size()), m_args.data());
            cdbInstantiatedCreatedPromise.set_value();
            return m_cdbInstance->exec();
        });
    cdbInstantiatedCreatedFuture.wait();
}

void CdbFunctionalTest::startAndWaitUntilStarted()
{
    start();
    waitUntilStarted();
}

void CdbFunctionalTest::waitUntilStarted()
{
    static const std::chrono::seconds CDB_INITIALIZED_MAX_WAIT_TIME(15);

    const auto endClock = std::chrono::steady_clock::now() + CDB_INITIALIZED_MAX_WAIT_TIME;
    for (;;)
    {
        ASSERT_TRUE(std::chrono::steady_clock::now() < endClock);

        ASSERT_NE(
            std::future_status::ready,
            m_cdbProcessFuture.wait_for(std::chrono::seconds(0)));

        auto connection = m_connectionFactory->createConnection("", "");
        api::ResultCode result = api::ResultCode::ok;
        std::tie(result, m_moduleInfo) = makeSyncCall<api::ResultCode, api::ModuleInfo>(
            std::bind(
                &nx::cdb::api::Connection::ping,
                connection.get(),
                std::placeholders::_1));
        if (result == api::ResultCode::ok)
            break;
    }
}

void CdbFunctionalTest::stop()
{
    m_cdbInstance->pleaseStop();
    m_cdbProcessFuture.wait();
    m_cdbInstance.reset();
}

void CdbFunctionalTest::restart()
{
    stop();
    start();
    waitUntilStarted();
}

void CdbFunctionalTest::addArg(const char* arg)
{
    auto b = std::back_inserter(m_args);
    *b = strdup(arg);
}

SocketAddress CdbFunctionalTest::endpoint() const
{
    return SocketAddress(HostAddress::localhost, m_port);
}

nx::cdb::api::ConnectionFactory* CdbFunctionalTest::connectionFactory()
{
    return m_connectionFactory.get();
}

api::ModuleInfo CdbFunctionalTest::moduleInfo() const
{
    return m_moduleInfo;
}

api::ResultCode CdbFunctionalTest::addAccount(
    api::AccountData* const accountData,
    std::string* const password,
    api::AccountConfirmationCode* const activationCode)
{
    if (accountData->email.empty())
    {
        std::ostringstream ss;
        ss << "test_" << rand() << "@networkoptix.com";
        accountData->email = ss.str();
    }

    if (password->empty())
    {
        std::ostringstream ss;
        ss << rand();
        *password = ss.str();
    }

    if (accountData->fullName.empty())
        accountData->fullName = "Test Test";
    if (accountData->passwordHa1.empty())
        accountData->passwordHa1 = nx_http::calcHa1(
            QUrl::fromPercentEncoding(QByteArray(accountData->email.c_str())).toLatin1().constData(),
            //accountData->email.c_str(),
            moduleInfo().realm.c_str(),
            password->c_str()).constData();
    if (accountData->customization.empty())
        accountData->customization = QN_CUSTOMIZATION_NAME;

    auto connection = connectionFactory()->createConnection("", "");

    //adding account
    api::ResultCode result = api::ResultCode::ok;
    std::tie(result, *activationCode) = makeSyncCall<api::ResultCode, api::AccountConfirmationCode>(
        std::bind(
            &nx::cdb::api::AccountManager::registerNewAccount,
            connection->accountManager(),
            *accountData,
            std::placeholders::_1));
    return result;
}

api::ResultCode CdbFunctionalTest::activateAccount(
    const api::AccountConfirmationCode& activationCode,
    std::string* const accountEmail)
{
    //activating account
    auto connection = connectionFactory()->createConnection("", "");

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

api::ResultCode CdbFunctionalTest::reactivateAccount(
    const std::string& email,
    api::AccountConfirmationCode* const activationCode)
{
    auto connection = connectionFactory()->createConnection("", "");

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

api::ResultCode CdbFunctionalTest::getAccount(
    const std::string& email,
    const std::string& password,
    api::AccountData* const accountData)
{
    auto connection = connectionFactory()->createConnection(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *accountData) =
        makeSyncCall<api::ResultCode, nx::cdb::api::AccountData>(
            std::bind(
                &nx::cdb::api::AccountManager::getAccount,
                connection->accountManager(),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbFunctionalTest::addActivatedAccount(
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

    assert(accountData->email == email);

    resCode = getAccount(
        accountData->email,
        *password,
        accountData);
    return resCode;
}

api::ResultCode CdbFunctionalTest::updateAccount(
    const std::string& email,
    const std::string& password,
    api::AccountUpdateData updateData)
{
    auto connection = connectionFactory()->createConnection(email, password);

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

api::ResultCode CdbFunctionalTest::resetAccountPassword(
    const std::string& email,
    std::string* const confirmationCode)
{
    auto connection = connectionFactory()->createConnection(email, std::string());

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

api::ResultCode CdbFunctionalTest::bindRandomSystem(
    const std::string& email,
    const std::string& password,
    api::SystemData* const systemData)
{
    auto connection = connectionFactory()->createConnection(email, password);

    api::SystemRegistrationData sysRegData;
    std::ostringstream ss;
    ss << "test_sys_" << rand();
    sysRegData.name = ss.str();

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

api::ResultCode CdbFunctionalTest::unbindSystem(
    const std::string& login,
    const std::string& password,
    const std::string& systemID)
{
    auto connection = connectionFactory()->createConnection(login, password);

    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::unbindSystem,
                connection->systemManager(),
                systemID,
                std::placeholders::_1));

    return resCode;
}

api::ResultCode CdbFunctionalTest::getSystems(
    const std::string& email,
    const std::string& password,
    std::vector<api::SystemData>* const systems)
{
    auto connection = connectionFactory()->createConnection(email, password);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemDataList systemDataList;
    std::tie(resCode, systemDataList) =
        makeSyncCall<api::ResultCode, api::SystemDataList>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystems,
                connection->systemManager(),
                std::placeholders::_1));
    *systems = std::move(systemDataList.systems);

    return resCode;
}

api::ResultCode CdbFunctionalTest::shareSystem(
    const std::string& email,
    const std::string& password,
    const std::string& systemID,
    const std::string& accountEmail,
    api::SystemAccessRole accessRole)
{
    auto connection = connectionFactory()->createConnection(email, password);

    api::SystemSharing systemSharing;
    systemSharing.accountEmail = accountEmail;
    systemSharing.systemID = systemID;
    systemSharing.accessRole = accessRole;

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

api::ResultCode CdbFunctionalTest::updateSystemSharing(
    const std::string& email,
    const std::string& password,
    const std::string& systemID,
    const std::string& accountEmail,
    api::SystemAccessRole newAccessRole)
{
    auto connection = connectionFactory()->createConnection(email, password);

    api::SystemSharing systemSharing;
    systemSharing.accountEmail = accountEmail;
    systemSharing.systemID = systemID;
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

api::ResultCode CdbFunctionalTest::getSystemSharings(
    const std::string& email,
    const std::string& password,
    std::vector<api::SystemSharing>* const sharings)
{
    auto connection = connectionFactory()->createConnection(email, password);

    typedef void(nx::cdb::api::SystemManager::*GetCloudUsersOfSystemType)
                (std::function<void(api::ResultCode, api::SystemSharingList)>);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemSharingList data;
    std::tie(resCode, data) =
        makeSyncCall<api::ResultCode, api::SystemSharingList>(
            std::bind(
                static_cast<GetCloudUsersOfSystemType>(
                    &nx::cdb::api::SystemManager::getCloudUsersOfSystem),
                connection->systemManager(),
                std::placeholders::_1));

    *sharings = std::move(data.sharing);
    return resCode;
}

api::ResultCode CdbFunctionalTest::getCdbNonce(
    const std::string& systemID,
    const std::string& authKey,
    api::NonceData* const nonceData)
{
    auto connection = connectionFactory()->createConnection(systemID, authKey);

    api::ResultCode resCode = api::ResultCode::ok;
    std::tie(resCode, *nonceData) =
        makeSyncCall<api::ResultCode, api::NonceData>(
            std::bind(
                &nx::cdb::api::AuthProvider::getCdbNonce,
                connection->authProvider(),
                std::placeholders::_1));
    return resCode;
}

api::ResultCode CdbFunctionalTest::ping(
    const std::string& systemID,
    const std::string& authKey)
{
    auto connection = connectionFactory()->createConnection(systemID, authKey);

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

api::ResultCode CdbFunctionalTest::getSystemSharings(
    const std::string& email,
    const std::string& password,
    const std::string& systemID,
    std::vector<api::SystemSharing>* const sharings)
{
    auto connection = connectionFactory()->createConnection(email, password);

    typedef void(nx::cdb::api::SystemManager::*GetCloudUsersOfSystemType)
        (const std::string&, std::function<void(api::ResultCode, api::SystemSharingList)>);

    api::ResultCode resCode = api::ResultCode::ok;
    api::SystemSharingList data;
    std::tie(resCode, data) =
        makeSyncCall<api::ResultCode, api::SystemSharingList>(
            std::bind(
                static_cast<GetCloudUsersOfSystemType>(
                    &nx::cdb::api::SystemManager::getCloudUsersOfSystem),
                connection->systemManager(),
                systemID,
                std::placeholders::_1));

    *sharings = std::move(data.sharing);
    return resCode;
}

api::SystemAccessRole CdbFunctionalTest::accountAccessRoleForSystem(
    const std::vector<api::SystemSharing>& sharings,
    const std::string& accountEmail,
    const std::string& systemID) const
{
    for (const auto& sharing: sharings)
    {
        if (sharing.accountEmail == accountEmail && sharing.systemID == systemID)
            return sharing.accessRole;
    }

    return api::SystemAccessRole::none;
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
}

}   //cdb
}   //nx

