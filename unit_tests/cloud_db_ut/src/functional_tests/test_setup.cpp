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
#include <utils/network/http/auth_tools.h>

#include "version.h"


namespace nx {
namespace cdb {

CdbFunctionalTest::CdbFunctionalTest()
:
    m_port(0),
    m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
    m_port = (std::rand() % 10000) + 50000;
    //m_port = 54526;
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
    
    start();
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
            m_cdbInstance = std::make_unique<nx::cdb::CloudDBProcess>(
                static_cast<int>(m_args.size()), m_args.data());
            cdbInstantiatedCreatedPromise.set_value();
            return m_cdbInstance->exec();
        });
    cdbInstantiatedCreatedFuture.wait();
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
    api::AccountActivationCode* const activationCode)
{
    std::ostringstream ss;
    if (accountData->email.empty())
    {
        ss << "test_" << rand() << "@networkoptix.com";
        accountData->email = ss.str();
    }

    if (password->empty())
    {
        ss = std::ostringstream();
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
            password->c_str());
    if (accountData->customization.empty())
        accountData->customization = QN_CUSTOMIZATION_NAME;

    auto connection = connectionFactory()->createConnection("", "");

    //adding account
    api::ResultCode result = api::ResultCode::ok;
    std::tie(result, *activationCode) = makeSyncCall<api::ResultCode, api::AccountActivationCode>(
        std::bind(
            &nx::cdb::api::AccountManager::registerNewAccount,
            connection->accountManager(),
            *accountData,
            std::placeholders::_1));
    return result;
}

api::ResultCode CdbFunctionalTest::activateAccount(
    const api::AccountActivationCode& activationCode)
{
    //activating account
    auto connection = connectionFactory()->createConnection("", "");

    //adding account
    api::ResultCode result = api::ResultCode::ok;
    std::tie(result) = makeSyncCall<api::ResultCode>(
        std::bind(
            &nx::cdb::api::AccountManager::activateAccount,
            connection->accountManager(),
            activationCode,
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
    api::AccountActivationCode activationCode;
    auto resCode = addAccount(
        accountData,
        password,
        &activationCode);
    if (resCode != api::ResultCode::ok)
        return resCode;

    resCode = activateAccount(activationCode);
    if (resCode != api::ResultCode::ok)
        return resCode;

    resCode = getAccount(
        accountData->email,
        *password,
        accountData);
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
    const QnUuid& systemID,
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
    const QnUuid& systemID,
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
                &nx::cdb::api::SystemManager::updateSharing,
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
    const QnUuid& systemID) const
{
    for (const auto& sharing: sharings)
    {
        if (sharing.accountEmail == accountEmail && sharing.systemID == systemID)
            return sharing.accessRole;
    }

    return api::SystemAccessRole::none;
}

}   //cdb
}   //nx
