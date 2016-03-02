/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include <QtSql/QSqlQuery>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <utils/common/guard.h>
#include <utils/common/sync_call.h>
#include <utils/serialization/lexical.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>

#include "account_manager.h"
#include "access_control/authentication_manager.h"
#include "access_control/authorization_manager.h"
#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {

SystemManager::SystemManager(
    const AccountManager& accountManager,
    nx::db::DBManager* const dbManager) throw(std::runtime_error)
:
    m_accountManager(accountManager),
    m_dbManager(dbManager)
{
    //pre-filling cache
    if (fillCache() != db::DBResult::ok)
        throw std::runtime_error("Failed to pre-load systems cache");
}

SystemManager::~SystemManager()
{
    m_startedAsyncCallsCounter.wait();
}

void SystemManager::authenticateByName(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& /*authSearchInputData*/,
    stree::ResourceContainer* const authProperties,
    std::function<void(bool)> completionHandler)
{
    bool result = false;
    auto scopedGuard = makeScopedGuard(
        [&completionHandler, &result]() {
            completionHandler(result);
        });

    QnMutexLocker lk(&m_mutex);

    auto systemData = m_cache.find(QnUuid::fromStringSafe(username));
    if (!systemData)
        return;

    if (!validateHa1Func(nx_http::calcHa1(
            username,
            AuthenticationManager::realm(),
            nx::String(systemData->authKey.c_str()))))
    {
        return;
    }

    auto systemID = QnUuid(systemData->id);

    authProperties->put(
        cdb::attr::authSystemID,
        QVariant::fromValue(systemID));

    m_cache.atomicUpdate(
        systemID,
        [](data::SystemData& system){ system.status = api::SystemStatus::ssActivated; });

    using namespace std::placeholders;

    m_dbManager->executeUpdate<QnUuid>(
        std::bind(&SystemManager::activateSystem, this, _1, _2),
        std::move(systemID),
        std::bind(&SystemManager::systemActivated, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2, [](api::ResultCode){}));

    result = true;
}

void SystemManager::bindSystemToAccount(
    const AuthorizationInfo& authzInfo,
    data::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, data::SystemData)> completionHandler)
{
    QString accountEmail;
    if (!authzInfo.get(cdb::attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::notAuthorized, data::SystemData());
        return;
    }

    data::SystemRegistrationDataWithAccount registrationDataWithAccount(
        std::move(registrationData));
    registrationDataWithAccount.accountEmail = accountEmail.toStdString();

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemRegistrationDataWithAccount, data::SystemData>(
        std::bind(&SystemManager::insertSystemToDB, this, _1, _2, _3),
        std::move(registrationDataWithAccount),
        std::bind(&SystemManager::systemAdded,
                    this, m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, std::move(completionHandler)));
}

void SystemManager::unbindSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemID>(
        std::bind(&SystemManager::deleteSystemFromDB, this, _1, _2),
        std::move(systemID),
        std::bind(&SystemManager::systemDeleted,
                    this, m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, std::move(completionHandler)));
}

namespace {
//!Returns \a true, if \a record contains every single resource present in \a filter 
bool applyFilter(
    const stree::AbstractResourceReader& record,
    const stree::AbstractIteratableContainer& filter)
{
    //TODO #ak this method should be moved to stree and have linear complexity, not n*log(n)!
    for (auto it = filter.begin(); !it->atEnd(); it->next())
    {
        auto val = record.get(it->resID());
        if (!val || val.get() != it->value())
            return false;
    }

    return true;
}
}

void SystemManager::getSystems(
    const AuthorizationInfo& authzInfo,
    data::DataFilter filter,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler )
{
    stree::MultiSourceResourceReader wholeFilterMap(filter, authzInfo);

    const auto accountEmail = wholeFilterMap.get<std::string>(cdb::attr::authAccountEmail);

    api::SystemDataExList resultData;
    auto systemID = wholeFilterMap.get<QnUuid>(cdb::attr::authSystemID);
    if (!systemID)
        systemID = wholeFilterMap.get<QnUuid>(cdb::attr::systemID);
    if (systemID)
    {
        auto systemIDVal = systemID.get().toString();
        //selecting system by id
        auto system = m_cache.find(systemID.get());
        if (!system || !applyFilter(system.get(), filter))
            return completionHandler(api::ResultCode::notFound, resultData);
        resultData.systems.emplace_back(std::move(system.get()));
    }
    else if (accountEmail)
    {
        QnMutexLocker lk(&m_mutex);
        const auto& accountIndex = m_accountAccessRoleForSystem.get<INDEX_BY_ACCOUNT_EMAIL>();
        const auto accountSystemsRange = accountIndex.equal_range(accountEmail.get());
        for (auto it = accountSystemsRange.first; it != accountSystemsRange.second; ++it)
        {
            auto system = m_cache.find(it->systemID);
            if (system)
                resultData.systems.push_back(system.get());
        }
    }
    else
    {
        //filtering full system list
        m_cache.forEach(
            [&](const std::pair<const QnUuid, data::SystemData>& data) {
                if (applyFilter(data.second, filter))
                    resultData.systems.push_back(data.second);
            });
    }

    if (accountEmail)
    {
        for (auto& systemDataEx: resultData.systems)
        {
            const auto accountData = 
                m_accountManager.findAccountByUserName(systemDataEx.ownerAccountEmail);
            if (accountData)
                systemDataEx.ownerFullName = accountData->fullName;
            systemDataEx.accessRole = getAccountRightsForSystem(
                *accountEmail,
                systemDataEx.id);
            systemDataEx.sharingPermissions = 
                std::move(getSharingPermissions(systemDataEx.accessRole).accessRoles);
        }
    }

    completionHandler(
        api::ResultCode::ok,
        resultData);
}

void SystemManager::shareSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemSharing>(
        std::bind(&SystemManager::updateSharingInDB, this, _1, _2),
        std::move(sharing),
        std::bind(&SystemManager::sharingUpdated,
            this, m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2, std::move(completionHandler)));
}

void SystemManager::getCloudUsersOfSystem(
    const AuthorizationInfo& authzInfo,
    const data::DataFilter& filter,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    api::SystemSharingExList resultData;

    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
        return completionHandler(api::ResultCode::notAuthorized, std::move(resultData));

    auto systemID = filter.get<QnUuid>(attr::authSystemID);
    if (!systemID)
        systemID = filter.get<QnUuid>(attr::systemID);

    QnMutexLocker lk(&m_mutex);
    if (systemID)
    {
        //selecting all sharings of system id
        const auto& systemIndex = m_accountAccessRoleForSystem.get<INDEX_BY_SYSTEM_ID>();
        const auto systemSharingsRange = systemIndex.equal_range(systemID.get());
        for (auto it = systemSharingsRange.first; it != systemSharingsRange.second; ++it)
        {
            api::SystemSharingEx sharingEx;
            sharingEx.api::SystemSharing::operator=(*it);
            resultData.sharing.emplace_back(std::move(sharingEx));
        }
    }
    else
    {
        //selecting sharings for every system account has access to
        const auto& accountIndex = m_accountAccessRoleForSystem.get<INDEX_BY_ACCOUNT_EMAIL>();
        const auto accountsSystemsRange = accountIndex.equal_range(accountEmail);
        for (auto accountIter = accountsSystemsRange.first;
             accountIter != accountsSystemsRange.second;
             ++accountIter)
        {
            if (accountIter->accessRole != api::SystemAccessRole::owner &&
                accountIter->accessRole != api::SystemAccessRole::cloudAdmin &&
                accountIter->accessRole != api::SystemAccessRole::maintenance)
            {
                //TODO #ak this condition copies condition from authorization tree
                //no access to system's sharing list is allowed for this account
                continue;
            }
            const auto& systemIndex = m_accountAccessRoleForSystem.get<INDEX_BY_SYSTEM_ID>();
            const auto systemSharingRange = systemIndex.equal_range(accountIter->systemID);
            for (auto sharingIter = systemSharingRange.first;
                 sharingIter != systemSharingRange.second;
                 ++sharingIter)
            {
                api::SystemSharingEx sharingEx;
                sharingEx.api::SystemSharing::operator=(*sharingIter);
                resultData.sharing.emplace_back(std::move(sharingEx));
            }
        }
    }
    lk.unlock();

    for (api::SystemSharingEx& sharingEx: resultData.sharing)
    {
        const auto account = 
            m_accountManager.findAccountByUserName(sharingEx.accountEmail);
        if (static_cast<bool>(account))
            sharingEx.fullName = account->fullName;
    }

    completionHandler(
        api::ResultCode::ok,
        std::move(resultData));
}

void SystemManager::getAccessRoleList(
    const AuthorizationInfo& authzInfo,
    data::SystemID systemID,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
        return completionHandler(
            api::ResultCode::notAuthorized,
            api::SystemAccessRoleList());

    const auto accessRole = getAccountRightsForSystem(accountEmail, systemID.systemID);
    NX_LOGX(lm("account %1, system %2, account rights on system %3").
            arg(accountEmail).arg(systemID.systemID).arg(QnLexical::serialized(accessRole)),
            cl_logDEBUG2);
    if (accessRole == api::SystemAccessRole::none)
    {
        NX_ASSERT(false);
        return completionHandler(
            api::ResultCode::notAuthorized,
            api::SystemAccessRoleList());
    }

    api::SystemAccessRoleList resultData = getSharingPermissions(accessRole);
    return completionHandler(
        api::ResultCode::ok,
        std::move(resultData));
}

boost::optional<data::SystemData> SystemManager::findSystemByID(const QnUuid& id) const
{
    return m_cache.find(id);
}

api::SystemAccessRole SystemManager::getAccountRightsForSystem(
    const std::string& accountEmail,
    const QnUuid& systemID) const
{
    QnMutexLocker lk(&m_mutex);
    const auto& accountSystemPairIndex = m_accountAccessRoleForSystem.get<0>();
    api::SystemSharing toFind;
    toFind.accountEmail = accountEmail;
    toFind.systemID = systemID;
    const auto it = accountSystemPairIndex.find(toFind);
    return it != accountSystemPairIndex.end()
        ? it->accessRole
        : api::SystemAccessRole::none;
}

nx::db::DBResult SystemManager::insertSystemToDB(
    QSqlDatabase* const connection,
    const data::SystemRegistrationDataWithAccount& newSystem,
    data::SystemData* const systemData)
{
    const auto account = m_accountManager.findAccountByUserName(newSystem.accountEmail);
    if (!account)
    {
        NX_LOG(lm("Failed to add system %1 to account %2. Account not found").
            arg(newSystem.name).arg(newSystem.accountEmail), cl_logDEBUG1);
        return db::DBResult::notFound;
    }

    QSqlQuery insertSystemQuery(*connection);
    insertSystemQuery.prepare(
        "INSERT INTO system( id, name, customization, auth_key, owner_account_id, status_code ) "
        " VALUES( :id, :name, :customization, :authKey, :ownerAccountID, :status )");
    systemData->id = QnUuid::createUuid();
    systemData->name = newSystem.name;
    systemData->customization = newSystem.customization;
    systemData->authKey = QnUuid::createUuid().toStdString();
    systemData->ownerAccountEmail = account->email;
    systemData->status = api::SystemStatus::ssNotActivated;
    QnSql::bind(*systemData, &insertSystemQuery);
    insertSystemQuery.bindValue(
        ":ownerAccountID",
        QnSql::serialized_field(account->id));
    if (!insertSystemQuery.exec())
    {
        NX_LOG(lm("Could not insert system %1 into DB. %2").
            arg(newSystem.name).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    data::SystemSharing systemSharing;
    systemSharing.accountEmail = newSystem.accountEmail;
    systemSharing.systemID = systemData->id;
    systemSharing.accessRole = api::SystemAccessRole::owner;
    auto result = insertSystemSharingToDB(connection, systemSharing);
    if (result != nx::db::DBResult::ok)
    {
        NX_LOG(lm("Could not insert system %1 to account %2 binding into DB. %3").
            arg(newSystem.name).arg(newSystem.accountEmail).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return result;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult dbResult,
    data::SystemRegistrationDataWithAccount systemRegistrationData,
    data::SystemData systemData,
    std::function<void(api::ResultCode, data::SystemData)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating cache
        m_cache.insert(systemData.id, systemData);
        //updating "systems by account id" index
        QnMutexLocker lk(&m_mutex);
        api::SystemSharing sharing;
        sharing.accountEmail = systemRegistrationData.accountEmail;
        sharing.systemID = systemData.id;
        sharing.accessRole = api::SystemAccessRole::owner;
        m_accountAccessRoleForSystem.emplace(std::move(sharing));
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError,
        std::move(systemData));
}

nx::db::DBResult SystemManager::insertSystemSharingToDB(
    QSqlDatabase* const connection,
    const data::SystemSharing& systemSharing)
{
    const auto account = m_accountManager.findAccountByUserName(systemSharing.accountEmail);
    if (!account)
    {
        NX_LOG(lm("Could not insert system %1 to account %2 binding into DB. Account not found").
            arg(systemSharing.systemID).
            arg(systemSharing.accountEmail), cl_logDEBUG1);
        return db::DBResult::notFound;
    }

    QSqlQuery insertSystemToAccountBinding(*connection);
    insertSystemToAccountBinding.prepare(
        "INSERT INTO system_to_account( account_id, system_id, access_role_id ) "
        " VALUES( :accountID, :systemID, :accessRole )");
    QnSql::bind(systemSharing, &insertSystemToAccountBinding);
    insertSystemToAccountBinding.bindValue(
        ":accountID",
        QnSql::serialized_field(account->id));
    if (!insertSystemToAccountBinding.exec())
    {
        NX_LOG(lm("Could not insert system %1 to account %2 binding into DB. %3").
            arg(systemSharing.systemID).
            arg(systemSharing.accountEmail).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemSharingAdded(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult dbResult,
    data::SystemSharing systemSharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating "systems by account id" index
        QnMutexLocker lk(&m_mutex);
        m_accountAccessRoleForSystem.emplace(systemSharing);
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::deleteSystemFromDB(
    QSqlDatabase* const connection,
    const data::SystemID& systemID)
{
    QSqlQuery removeSystemToAccountBinding(*connection);
    removeSystemToAccountBinding.prepare(
        "DELETE FROM system_to_account WHERE system_id=:systemID");
    QnSql::bind(systemID, &removeSystemToAccountBinding);
    if (!removeSystemToAccountBinding.exec())
    {
        NX_LOG(lm("Could not delete system %1 from system_to_account. %2").
            arg(systemID.systemID).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    QSqlQuery removeSystem(*connection);
    removeSystem.prepare(
        "DELETE FROM system WHERE id=:systemID");
    QnSql::bind(systemID, &removeSystem);
    if (!removeSystem.exec())
    {
        NX_LOG(lm("Could not delete system %1. %2").
            arg(systemID.systemID).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult dbResult,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        m_cache.erase(systemID.systemID);
        QnMutexLocker lk(&m_mutex);
        auto& systemIndex = m_accountAccessRoleForSystem.get<2>();
        systemIndex.erase(systemID.systemID);
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::updateSharingInDB(
    QSqlDatabase* const connection,
    const data::SystemSharing& sharing)
{
    const auto account = m_accountManager.findAccountByUserName(sharing.accountEmail);
    if (!account)
    {
        NX_LOG(lm("Failed to update/remove sharing. system %1, account %2. Account not found").
            arg(sharing.systemID).arg(sharing.accountEmail), cl_logDEBUG1);
        return db::DBResult::notFound;
    }

    QSqlQuery updateRemoveSharingQuery(*connection);
    if (sharing.accessRole == api::SystemAccessRole::none)
        updateRemoveSharingQuery.prepare(
            "DELETE FROM system_to_account "
            "  WHERE account_id=:accountID AND system_id=:systemID");
    else
        updateRemoveSharingQuery.prepare(
            "INSERT OR REPLACE INTO system_to_account( account_id, system_id, access_role_id ) "
            " VALUES( :accountID, :systemID, :accessRole )");
    QnSql::bind(sharing, &updateRemoveSharingQuery);
    updateRemoveSharingQuery.bindValue(
        ":accountID",
        QnSql::serialized_field(account->id));
    if (!updateRemoveSharingQuery.exec())
    {
        NX_LOG(lm("Failed to update/remove sharing. system %1, account %2, access role %3. %4").
            arg(sharing.systemID).arg(sharing.accountEmail).
            arg(QnLexical::serialized(sharing.accessRole)).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::sharingUpdated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating "systems by account id" index
        QnMutexLocker lk(&m_mutex);
        auto& accountSystemPairIndex = m_accountAccessRoleForSystem.get<0>();
        accountSystemPairIndex.erase(sharing);
        if (sharing.accessRole != api::SystemAccessRole::none)
        {
            //inserting modified version
            accountSystemPairIndex.emplace(sharing);
        }

        return completionHandler(api::ResultCode::ok);
    }

    completionHandler(
        dbResult == nx::db::DBResult::notFound
        ? api::ResultCode::notFound
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::activateSystem(
    QSqlDatabase* const connection,
    const QnUuid& systemID)
{
    QSqlQuery updateSystemStatusQuery(*connection);
    updateSystemStatusQuery.prepare(
        "UPDATE system "
        "SET status_code=:statusCode "
        "WHERE id=:id");
    updateSystemStatusQuery.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(api::SystemStatus::ssActivated)));
    updateSystemStatusQuery.bindValue(
        ":id",
        QnSql::serialized_field(systemID));
    if (!updateSystemStatusQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void SystemManager::systemActivated(
    QnCounter::ScopedIncrement asyncCallLocker,
    nx::db::DBResult dbResult,
    QnUuid systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        m_cache.atomicUpdate(
            systemID,
            [](data::SystemData& system) { system.status = api::SystemStatus::ssActivated; });
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

api::SystemAccessRoleList SystemManager::getSharingPermissions(
    api::SystemAccessRole accessRole) const
{
    api::SystemAccessRoleList resultData;
    resultData.accessRoles.reserve(4);
    switch (accessRole)
    {
        case api::SystemAccessRole::none:
            NX_ASSERT(false);
            break;

        case api::SystemAccessRole::maintenance:
            resultData.accessRoles.emplace_back(api::SystemAccessRole::maintenance);
            break;

        case api::SystemAccessRole::owner:
            resultData.accessRoles.emplace_back(api::SystemAccessRole::maintenance);

        case api::SystemAccessRole::cloudAdmin:
            resultData.accessRoles.emplace_back(api::SystemAccessRole::liveViewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::viewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::advancedViewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::localAdmin);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::cloudAdmin);
            break;

        case api::SystemAccessRole::liveViewer:
        case api::SystemAccessRole::viewer:
        case api::SystemAccessRole::advancedViewer:
        case api::SystemAccessRole::localAdmin:
            break;
    }

    return resultData;
}

nx::db::DBResult SystemManager::fillCache()
{
    {
        std::promise<db::DBResult> cacheFilledPromise;
        auto future = cacheFilledPromise.get_future();

        //starting async operation
        using namespace std::placeholders;
        m_dbManager->executeSelect<int>(
            std::bind(&SystemManager::fetchSystems, this, _1, _2),
            [&cacheFilledPromise](db::DBResult dbResult, int /*dummy*/) {
                cacheFilledPromise.set_value(dbResult);
            });
        //waiting for completion
        future.wait();
        if (future.get() != db::DBResult::ok)
            return future.get();
    }

    //fetching system_to_account binding
    {
        std::promise<db::DBResult> cacheFilledPromise;
        auto future = cacheFilledPromise.get_future();

        //starting async operation
        using namespace std::placeholders;
        m_dbManager->executeSelect<int>(
            std::bind(&SystemManager::fetchSystemToAccountBinder, this, _1, _2),
            [&cacheFilledPromise](db::DBResult dbResult, int /*dummy*/) {
                cacheFilledPromise.set_value(dbResult);
            });
        //waiting for completion
        future.wait();
        if (future.get() != db::DBResult::ok)
            return future.get();
    }

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchSystems(QSqlDatabase* connection, int* const /*dummy*/)
{
    QSqlQuery readSystemsQuery(*connection);
    readSystemsQuery.prepare(
        "SELECT s.id, s.name, s.customization, s.auth_key as authKey, "
        "       a.email as ownerAccountEmail, s.status_code as status "
        "FROM system s, account a "
        "WHERE s.owner_account_id = a.id");
    if (!readSystemsQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }
    
    std::vector<data::SystemData> systems;
    QnSql::fetch_many(readSystemsQuery, &systems);

    for (auto& system : systems)
    {
        auto idCopy = system.id;
        if (!m_cache.insert(std::move(idCopy), std::move(system)))
        {
            NX_ASSERT(false);
        }

        //TODO #ak other indexes may be required also (e.g., systems by account, systems by name, etc..)
    }

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchSystemToAccountBinder(QSqlDatabase* connection, int* const /*dummy*/)
{
    QSqlQuery readSystemToAccountQuery(*connection);
    readSystemToAccountQuery.prepare(
        "SELECT a.email as accountEmail, "
               "sa.system_id as systemID, "
               "sa.access_role_id as accessRole "
        "FROM system_to_account sa, account a "
        "WHERE sa.account_id = a.id");
    if (!readSystemToAccountQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<data::SystemSharing> systemToAccount;
    QnSql::fetch_many(readSystemToAccountQuery, &systemToAccount);
    for (auto& sharing: systemToAccount)
        m_accountAccessRoleForSystem.emplace(std::move(sharing));

    return db::DBResult::ok;
}

}   //cdb
}   //nx
