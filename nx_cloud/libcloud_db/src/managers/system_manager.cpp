/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include <limits>

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_resource_data.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>
#include <utils/common/guard.h>
#include <utils/common/id.h>
#include <utils/common/sync_call.h>

#include "access_control/authentication_manager.h"
#include "access_control/authorization_manager.h"
#include "account_manager.h"
#include "ec2/data_conversion.h"
#include "ec2/incoming_transaction_dispatcher.h"
#include "ec2/transaction_log.h"
#include "system_health_info_provider.h"
#include "settings.h"
#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {

static api::SystemSharingEx createDerivedFromBase(api::SystemSharing right)
{
    api::SystemSharingEx result;
    result.api::SystemSharing::operator=(std::move(right));
    return result;
}

SystemManager::SystemManager(
    const conf::Settings& settings,
    nx::utils::TimerManager* const timerManager,
    AccountManager* const accountManager,
    const SystemHealthInfoProvider& systemHealthInfoProvider,
    nx::db::AsyncSqlQueryExecutor* const dbManager,
    ec2::TransactionLog* const transactionLog,
    ec2::IncomingTransactionDispatcher* const transactionDispatcher) throw(std::runtime_error)
:
    m_settings(settings),
    m_timerManager(timerManager),
    m_accountManager(accountManager),
    m_systemHealthInfoProvider(systemHealthInfoProvider),
    m_dbManager(dbManager),
    m_transactionLog(transactionLog),
    m_transactionDispatcher(transactionDispatcher),
    m_dropSystemsTimerId(0),
    m_dropExpiredSystemsTaskStillRunning(false)
{
    // Pre-filling cache.
    if (fillCache() != db::DBResult::ok)
        throw std::runtime_error("Failed to pre-load systems cache");

    using namespace std::placeholders;
    m_dropSystemsTimerId =
        m_timerManager->addNonStopTimer(
            std::bind(&SystemManager::dropExpiredSystems, this, _1),
            m_settings.systemManager().dropExpiredSystemsPeriod,
            std::chrono::seconds::zero());

    // Registering transaction handler.
    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::saveUser, ::ec2::ApiUserData, data::SystemSharing>(
            std::bind(&SystemManager::processEc2SaveUser, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2SaveUserDone, this, _1, _2, _3));

    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::removeUser, ::ec2::ApiIdData, data::SystemSharing>(
            std::bind(&SystemManager::processEc2RemoveUser, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2RemoveUserDone, this, _1, _2, _3));

    // Currently this transaction can only rename some system.
    m_transactionDispatcher->registerTransactionHandler
        <::ec2::ApiCommand::setResourceParam,
         ::ec2::ApiResourceParamWithRefData,
         data::SystemNameUpdate>(
            std::bind(&SystemManager::processSetResourceParam, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2SetResourceParamDone, this, _1, _2, _3));
}

SystemManager::~SystemManager()
{
    m_timerManager->joinAndDeleteTimer(m_dropSystemsTimerId);

    m_startedAsyncCallsCounter.wait();
}

void SystemManager::authenticateByName(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& /*authSearchInputData*/,
    stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    api::ResultCode result = api::ResultCode::notAuthorized;
    auto scopedGuard = makeScopedGuard(
        [&completionHandler, &result]() {
            completionHandler(result);
        });

    QnMutexLocker lk(&m_mutex);

    auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    const auto systemIter = systemByIdIndex.find(
        std::string(username.constData(), username.size()));
    if (systemIter == systemByIdIndex.end())
        return;

    if (systemIter->status == api::SystemStatus::ssDeleted)
    {
        if (systemIter->expirationTimeUtc > ::time(NULL) ||
            m_settings.systemManager().controlSystemStatusByDb) //system not expired yet
        {
            result = api::ResultCode::credentialsRemovedPermanently;
        }
        return;
    }

    if (!validateHa1Func(
            nx_http::calcHa1(
                username,
                AuthenticationManager::realm(),
                nx::String(systemIter->authKey.c_str()))))
    {
        return;
    }

    authProperties->put(
        cdb::attr::authSystemID,
        QString::fromStdString(systemIter->id));

    systemByIdIndex.modify(
        systemIter,
        [](data::SystemData& system){ system.status = api::SystemStatus::ssActivated; });

    using namespace std::placeholders;

    m_dbManager->executeUpdate<std::string>(
        std::bind(&SystemManager::activateSystem, this, _1, _2),
        systemIter->id,
        std::bind(&SystemManager::systemActivated, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2, _3, [](api::ResultCode){}));

    result = api::ResultCode::ok;
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

    auto registrationDataWithAccount = 
        std::make_unique<data::SystemRegistrationDataWithAccount>(
            std::move(registrationData));
    registrationDataWithAccount->accountEmail = accountEmail.toStdString();
    auto registrationDataWithAccountPtr = registrationDataWithAccount.get();
    auto newSystemData = std::make_unique<InsertNewSystemToDbResult>();
    auto newSystemDataPtr = newSystemData.get();
    newSystemData->systemData.id = QnUuid::createUuid().toSimpleString().toStdString();   //guid without {}

    auto dbUpdateFunc = 
        [this, registrationDataWithAccountPtr, newSystemDataPtr](
            QSqlDatabase* const connection) -> nx::db::DBResult
        {
            return insertSystemToDB(
                connection,
                *registrationDataWithAccountPtr,
                newSystemDataPtr);
        };

    auto onDbUpdateCompletedFunc = 
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            registrationDataWithAccount = std::move(registrationDataWithAccount),
            newSystemData = std::move(newSystemData),
            completionHandler = std::move(completionHandler)](
                QSqlDatabase* dbConnection,
                nx::db::DBResult dbResult)
        {
            systemAdded(
                std::move(locker),
                dbConnection,
                dbResult,
                std::move(*registrationDataWithAccount),
                std::move(*newSystemData),
                std::move(completionHandler));
        };

    // Creating db transaction via TransactionLog.
    m_transactionLog->startDbTransaction(
        newSystemDataPtr->systemData.id.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::unbindSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string>(
        std::bind(&SystemManager::markSystemAsDeleted, this, _1, _2),
        std::move(systemID.systemID),
        std::bind(&SystemManager::systemMarkedAsDeleted,
                    this, m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, std::move(completionHandler)));
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
    //always providing only activated systems
    filter.resources().put(
        attr::systemStatus,
        static_cast<int>(api::SystemStatus::ssActivated));

    stree::MultiSourceResourceReader wholeFilterMap(filter, authzInfo);

    const auto accountEmail = wholeFilterMap.get<std::string>(cdb::attr::authAccountEmail);

    api::SystemDataExList resultData;
    auto systemID = wholeFilterMap.get<std::string>(cdb::attr::authSystemID);
    if (!systemID)
        systemID = wholeFilterMap.get<std::string>(cdb::attr::systemID);

    QnMutexLocker lk(&m_mutex);
    const auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    if (systemID)
    {
        //selecting system by id
        auto systemIter = systemByIdIndex.find(systemID.get());
        if ((systemIter == systemByIdIndex.end()) ||
            !applyFilter(*systemIter, filter))
        {
            lk.unlock();
            return completionHandler(api::ResultCode::notFound, resultData);
        }
        resultData.systems.emplace_back(*systemIter);
    }
    else if (accountEmail)
    {
        const auto& accountIndex = m_accountAccessRoleForSystem.get<kSharingByAccountEmail>();
        const auto accountSystemsRange = accountIndex.equal_range(accountEmail.get());
        for (auto it = accountSystemsRange.first; it != accountSystemsRange.second; ++it)
        {
            auto systemIter = systemByIdIndex.find(it->systemID);
            if ((systemIter != systemByIdIndex.end()) &&
                applyFilter(*systemIter, filter))
            {
                resultData.systems.push_back(*systemIter);
            }
        }
    }
    else
    {
        //filtering full system list
        for (const auto& system: systemByIdIndex)
        {
            if (applyFilter(system, filter))
                resultData.systems.push_back(system);
        }
    }
    lk.unlock();

    if (accountEmail)
    {
        //returning rights account has for each system
        for (auto& systemDataEx: resultData.systems)
        {
            const auto accountData = 
                m_accountManager->findAccountByUserName(systemDataEx.ownerAccountEmail);
            if (accountData)
                systemDataEx.ownerFullName = accountData->fullName;
            const auto sharingData = getSystemSharingData(
                *accountEmail,
                systemDataEx.id);
            if (sharingData)
            {
                systemDataEx.accessRole = sharingData->accessRole;
                systemDataEx.sortingOrder = sharingData->systemAccessWeight;
            }
            else
            {
                systemDataEx.accessRole = api::SystemAccessRole::none;
            }
            systemDataEx.sharingPermissions = 
                std::move(getSharingPermissions(systemDataEx.accessRole).accessRoles);
        }
    }

    //adding system health
    for (auto& systemDataEx : resultData.systems)
    {
        systemDataEx.stateOfHealth = 
            m_systemHealthInfoProvider.isSystemOnline(systemDataEx.id)
            ? api::SystemHealth::online
            : api::SystemHealth::offline;
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

    Qn::GlobalPermissions permissions(Qn::NoPermissions);
    if (sharing.customPermissions.empty())
    {
        bool isAdmin = false;
        ec2::accessRoleToPermissions(sharing.accessRole, &permissions, &isAdmin);
        sharing.customPermissions = QnLexical::serialized(permissions).toStdString();
    }
    sharing.vmsUserId = guidFromArbitraryData(
        sharing.accountEmail).toSimpleString().toStdString();

    auto sharingOHeap = std::make_unique<data::SystemSharing>(std::move(sharing));
    data::SystemSharing* sharingOHeapPtr = sharingOHeap.get();

    auto dbUpdateFunc = 
        [this, sharingOHeapPtr](QSqlDatabase* const connection) -> nx::db::DBResult
        {
            return updateSharingInDbAndGenerateTransaction(
                connection,
                *sharingOHeapPtr);
        };

    auto onDbUpdateCompletedFunc = 
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            sharingOHeap = std::move(sharingOHeap),
            completionHandler = std::move(completionHandler)](
                QSqlDatabase* dbConnection,
                nx::db::DBResult dbResult)
        {
            sharingUpdated(
                std::move(locker),
                dbConnection,
                dbResult,
                std::move(*sharingOHeap),
                std::move(completionHandler));
        };

    m_transactionLog->startDbTransaction(
        sharing.systemID.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::getCloudUsersOfSystem(
    const AuthorizationInfo& authzInfo,
    const data::DataFilter& filter,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    api::SystemSharingExList resultData;

    auto authSystemID = authzInfo.get<std::string>(attr::authSystemID);

    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        if (!authSystemID)
            return completionHandler(api::ResultCode::notAuthorized, std::move(resultData));
    }

    auto systemID = authSystemID;
    if (!systemID)
        systemID = filter.get<std::string>(attr::systemID);

    QnMutexLocker lk(&m_mutex);

    if (systemID)
    {
        //selecting all sharings of system id
        const auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        const auto systemSharingsRange = systemIndex.equal_range(systemID.get());
        for (auto it = systemSharingsRange.first; it != systemSharingsRange.second; ++it)
        {
            resultData.sharing.emplace_back(*it);
        }
    }
    else
    {
        //selecting sharings for every system account has access to
        const auto& accountIndex = m_accountAccessRoleForSystem.get<kSharingByAccountEmail>();
        const auto accountsSystemsRange = accountIndex.equal_range(accountEmail);
        for (auto accountIter = accountsSystemsRange.first;
             accountIter != accountsSystemsRange.second;
             ++accountIter)
        {
            if (accountIter->accessRole != api::SystemAccessRole::owner &&
                accountIter->accessRole != api::SystemAccessRole::cloudAdmin &&
                accountIter->accessRole != api::SystemAccessRole::localAdmin &&
                accountIter->accessRole != api::SystemAccessRole::maintenance)
            {
                //TODO #ak this condition copies condition from authorization tree
                //no access to system's sharing list is allowed for this account
                continue;
            }

            const auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
            const auto systemSharingRange = systemIndex.equal_range(accountIter->systemID);
            for (auto sharingIter = systemSharingRange.first;
                 sharingIter != systemSharingRange.second;
                 ++sharingIter)
            {
                resultData.sharing.emplace_back(*sharingIter);
            }
        }
    }
    lk.unlock();

    for (api::SystemSharingEx& sharingEx: resultData.sharing)
    {
        const auto account = 
            m_accountManager->findAccountByUserName(sharingEx.accountEmail);
        if (static_cast<bool>(account))
        {
            sharingEx.accountFullName = account->fullName;
            sharingEx.accountID = account->id;
        }
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

constexpr const std::size_t kMaxSystemNameLength = 1024;

void SystemManager::rename(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemNameUpdate data,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_LOGX(lm("Rename system %1 to %2").arg(data.systemID).arg(data.name), cl_logDEBUG2);

    if (data.name.empty() || data.name.size() > kMaxSystemNameLength)
        return completionHandler(api::ResultCode::badRequest);

    //if system name matches current, no update is needed
    {
        QnMutexLocker lk(&m_mutex);

        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        auto systemIter = systemByIdIndex.find(data.systemID);
        if (systemIter == systemByIdIndex.end())
        {
            NX_ASSERT(false);
            return completionHandler(api::ResultCode::notFound);
        }

        if (systemIter->name == data.name)
        {
            //system name match existing
            return completionHandler(api::ResultCode::ok);
        }
    }

    auto systemNameDataOnHeap = std::make_unique<data::SystemNameUpdate>(std::move(data));
    auto systemNameDataOnHeapPtr = systemNameDataOnHeap.get();

    auto dbUpdateFunc =
        [this, systemNameDataOnHeapPtr](
            QSqlDatabase* const connection) -> nx::db::DBResult
    {
        return updateSystemNameInDB(
            connection,
            *systemNameDataOnHeapPtr);
    };

    auto onDbUpdateCompletedFunc =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            systemNameData = std::move(systemNameDataOnHeap),
            completionHandler = std::move(completionHandler)](
                QSqlDatabase* dbConnection,
                nx::db::DBResult dbResult) mutable
        {
            systemNameUpdated(
                std::move(locker),
                dbConnection,
                dbResult,
                std::move(*systemNameData),
                std::move(completionHandler));
        };

    m_transactionLog->startDbTransaction(
        systemNameDataOnHeapPtr->systemID.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::recordUserSessionStart(
    const AuthorizationInfo& authzInfo,
    data::UserSessionDescriptor userSessionDescriptor,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_ASSERT(userSessionDescriptor.systemId || userSessionDescriptor.accountEmail);
    if (!(userSessionDescriptor.systemId || userSessionDescriptor.accountEmail))
        return completionHandler(api::ResultCode::unknownError);

    if (!userSessionDescriptor.systemId)
    {
        userSessionDescriptor.systemId = authzInfo.get<std::string>(attr::authSystemID);
        if (!userSessionDescriptor.systemId)
        {
            NX_LOGX(lm("system id may be absent in request parameters "
                "only if it has been used as a login"),
                cl_logDEBUG1);
            return completionHandler(api::ResultCode::forbidden);
        }
    }

    if (!userSessionDescriptor.accountEmail)
    {
        userSessionDescriptor.accountEmail = 
            authzInfo.get<std::string>(attr::authAccountEmail);
        if (!userSessionDescriptor.accountEmail)
        {
            NX_LOGX(lm("account email may be absent in request parameters "
                "only if it has been used as a login"),
                cl_logDEBUG1);
            return completionHandler(api::ResultCode::forbidden);
        }
    }

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::UserSessionDescriptor, double>(
        std::bind(&SystemManager::updateSystemAccessWeightInDb, this, _1, _2, _3),
        std::move(userSessionDescriptor),
        std::bind(&SystemManager::systemAccessWeightUpdatedInDb, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2, _3, _4, std::move(completionHandler)));
}

api::SystemAccessRole SystemManager::getAccountRightsForSystem(
    const std::string& accountEmail,
    const std::string& systemID) const
{
#if 0
    QnMutexLocker lk(&m_mutex);
    const auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    api::SystemSharing toFind;
    toFind.accountEmail = accountEmail;
    toFind.systemID = systemID;
    const auto it = accountSystemPairIndex.find(toFind);
    return it != accountSystemPairIndex.end()
        ? it->accessRole
        : api::SystemAccessRole::none;
#else
    //TODO #ak getSystemSharingData does returns a lot of data which is redundant for this method.
    const auto sharingData = getSystemSharingData(accountEmail, systemID);
    if (!sharingData)
        return api::SystemAccessRole::none;
    return sharingData->accessRole;
#endif
}

boost::optional<api::SystemSharingEx> SystemManager::getSystemSharingData(
    const std::string& accountEmail,
    const std::string& systemID) const
{
    QnMutexLocker lk(&m_mutex);
    const auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    //TODO #ak introduce index by (accountEmail, systemID) to m_accountAccessRoleForSystem?
    api::SystemSharing keyToFind;
    keyToFind.accountEmail = accountEmail;
    keyToFind.systemID = systemID;
    const auto it = accountSystemPairIndex.find(keyToFind);
    if (it == accountSystemPairIndex.end())
        return boost::none;

    api::SystemSharingEx resultData = *it;
    //resultData.api::SystemSharing::operator=(*it);

    const auto account = m_accountManager->findAccountByUserName(accountEmail);
    if (!account)
        return boost::none;

    resultData.accountID = account->id;
    resultData.accountFullName = account->fullName;
    return resultData;
}

std::pair<std::string, std::string> SystemManager::extractSystemIdAndVmsUserId(
    const api::SystemSharing& data)
{
    return std::make_pair(data.systemID, data.vmsUserId);
}

std::pair<std::string, std::string> SystemManager::extractSystemIdAndAccountEmail(
    const api::SystemSharing& data)
{
    return std::make_pair(data.systemID, data.accountEmail);
}

nx::db::DBResult SystemManager::insertSystemToDB(
    QSqlDatabase* const connection,
    const data::SystemRegistrationDataWithAccount& newSystem,
    InsertNewSystemToDbResult* const result)
{
    nx::db::DBResult dbResult = nx::db::DBResult::ok;

    dbResult = insertNewSystemDataToDb(connection, newSystem, result);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    dbResult = m_transactionLog->updateTimestampHiForSystem(
        connection,
        result->systemData.id.c_str(),
        result->systemData.systemSequence);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    dbResult = insertOwnerSharingToDb(
        connection, result->systemData.id, newSystem, &result->ownerSharing);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::insertNewSystemDataToDb(
    QSqlDatabase* const connection,
    const data::SystemRegistrationDataWithAccount& newSystem,
    InsertNewSystemToDbResult* const result)
{
    const auto account = m_accountManager->findAccountByUserName(newSystem.accountEmail);
    if (!account)
    {
        NX_LOG(lm("Failed to add system %1 (%2) to account %3. Account not found")
            .arg(newSystem.name).arg(result->systemData.id).arg(newSystem.accountEmail),
            cl_logDEBUG1);
        return db::DBResult::notFound;
    }

    QSqlQuery insertSystemQuery(*connection);
    insertSystemQuery.prepare(
        "INSERT INTO system(id, name, customization, auth_key, owner_account_id, "
                           "status_code, expiration_utc_timestamp) "
        " VALUES(:id, :name, :customization, :authKey, :ownerAccountID, "
                ":status, :expiration_utc_timestamp)");
    NX_ASSERT(!result->systemData.id.empty());
    result->systemData.name = newSystem.name;
    result->systemData.customization = newSystem.customization;
    result->systemData.authKey = QnUuid::createUuid().toSimpleString().toStdString();
    result->systemData.ownerAccountEmail = account->email;
    result->systemData.status = api::SystemStatus::ssNotActivated;
    result->systemData.expirationTimeUtc =
        ::time(NULL) +
        std::chrono::duration_cast<std::chrono::seconds>(
            m_settings.systemManager().notActivatedSystemLivePeriod).count();
    QnSql::bind(result->systemData, &insertSystemQuery);
    insertSystemQuery.bindValue(
        ":ownerAccountID",
        QnSql::serialized_field(account->id));
    insertSystemQuery.bindValue(
        ":expiration_utc_timestamp",
        result->systemData.expirationTimeUtc);
    if (!insertSystemQuery.exec())
    {
        NX_LOG(lm("Could not insert system %1 (%2) into DB. %3")
            .arg(newSystem.name).arg(result->systemData.id)
            .arg(insertSystemQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    // Selecting generated system sequence
    QSqlQuery selectSystemSequence(*connection);
    selectSystemSequence.setForwardOnly(true);
    selectSystemSequence.prepare(
        R"sql(
        SELECT seq FROM system WHERE id=?
        )sql");
    selectSystemSequence.bindValue(0, QnSql::serialized_field(result->systemData.id));
    if (!selectSystemSequence.exec() ||
        !selectSystemSequence.next())
    {
        NX_LOG(lm("Error selecting sequence of newly-created system %1 (%2). %3")
            .arg(newSystem.name).arg(result->systemData.id)
            .arg(insertSystemQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }
    result->systemData.systemSequence = selectSystemSequence.value(0).toULongLong();

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::insertOwnerSharingToDb(
    QSqlDatabase* const connection,
    const std::string& systemId,
    const data::SystemRegistrationDataWithAccount& newSystem,
    nx::cdb::data::SystemSharing* const ownerSharing)
{
    // Adding "owner" user to newly created system.
    ownerSharing->accountEmail = newSystem.accountEmail;
    ownerSharing->systemID = systemId;
    ownerSharing->accessRole = api::SystemAccessRole::owner;
    ownerSharing->vmsUserId = guidFromArbitraryData(
        ownerSharing->accountEmail).toSimpleString().toStdString();
    ownerSharing->isEnabled = true;
    ownerSharing->customPermissions = QnLexical::serialized(
        static_cast<Qn::GlobalPermissions>(Qn::GlobalAdminPermissionSet)).toStdString();
    const auto resultCode =
        updateSharingInDbAndGenerateTransaction(connection, *ownerSharing);
    if (resultCode != nx::db::DBResult::ok)
    {
        NX_LOG(lm("Could not insert system %1 to account %2 binding into DB. %3").
            arg(newSystem.name).arg(newSystem.accountEmail).
            arg(connection->lastError().text()), cl_logDEBUG1);
        return resultCode;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemRegistrationDataWithAccount /*systemRegistrationData*/,
    InsertNewSystemToDbResult resultData,
    std::function<void(api::ResultCode, data::SystemData)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);
        //updating cache
        m_systems.insert(resultData.systemData);
        //updating "systems by account id" index
        m_accountAccessRoleForSystem.emplace(createDerivedFromBase(std::move(resultData.ownerSharing)));
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError,
        std::move(resultData.systemData));
}

void SystemManager::systemSharingAdded(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemSharing systemSharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating "systems by account id" index
        QnMutexLocker lk(&m_mutex);
        m_accountAccessRoleForSystem.emplace(createDerivedFromBase(std::move(systemSharing)));
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::markSystemAsDeleted(
    QSqlDatabase* const connection,
    const std::string& systemId)
{
    //marking system as "deleted"
    QSqlQuery markSystemAsRemoved(*connection);
    markSystemAsRemoved.prepare(
        "UPDATE system "
        "SET status_code=:statusCode, "
            "expiration_utc_timestamp=:expiration_utc_timestamp "
        "WHERE id=:id");
    markSystemAsRemoved.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(api::SystemStatus::ssDeleted)));
    markSystemAsRemoved.bindValue(
        ":expiration_utc_timestamp",
        (int)(::time(NULL) +
            std::chrono::duration_cast<std::chrono::seconds>(
                m_settings.systemManager().reportRemovedSystemPeriod).count()));
    markSystemAsRemoved.bindValue(
        ":id",
        QnSql::serialized_field(systemId));
    if (!markSystemAsRemoved.exec())
    {
        NX_LOG(lm("Error marking system %1 as deleted. %2")
            .arg(systemId).arg(markSystemAsRemoved.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    //removing system-to-account
    QSqlQuery removeSystemToAccountBinding(*connection);
    removeSystemToAccountBinding.prepare(
        "DELETE FROM system_to_account WHERE system_id=:systemID");
    removeSystemToAccountBinding.bindValue(
        ":systemID",
        QnSql::serialized_field(systemId));
    if (!removeSystemToAccountBinding.exec())
    {
        NX_LOG(lm("Could not delete system %1 from system_to_account. %2").
            arg(systemId).arg(removeSystemToAccountBinding.lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemMarkedAsDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    std::string systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        auto systemIter = systemByIdIndex.find(systemId);
        if (systemIter != systemByIdIndex.end())
        {
            systemByIdIndex.modify(
                systemIter,
                [this](data::SystemData& system)
                { 
                    system.status = api::SystemStatus::ssDeleted;
                    system.expirationTimeUtc =
                        ::time(NULL) +
                        std::chrono::duration_cast<std::chrono::seconds>(
                            m_settings.systemManager().reportRemovedSystemPeriod).count();
                });
        }

        //removing system-to-account
        auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        systemIndex.erase(systemId);
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
        NX_LOG(lm("Could not delete system %1 from system_to_account. %2")
            .arg(systemID.systemID).arg(removeSystemToAccountBinding.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    QSqlQuery removeSystem(*connection);
    removeSystem.prepare(
        "DELETE FROM system WHERE id=:systemID");
    QnSql::bind(systemID, &removeSystem);
    if (!removeSystem.exec())
    {
        NX_LOG(lm("Could not delete system %1. %2")
            .arg(systemID.systemID).arg(removeSystem.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemID systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        systemByIdIndex.erase(systemID.systemID);
        auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        systemIndex.erase(systemID.systemID);
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::updateSharingInDb(
    QSqlDatabase* const connection,
    const data::SystemSharing& sharing,
    data::AccountData* const targetAccountData)
{
    nx::db::DBResult result = nx::db::DBResult::ok;

    {
        // Initializing account customization for case if account has to be created.
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        const auto systemIter = systemByIdIndex.find(sharing.systemID);
        if (systemIter != systemByIdIndex.end())
            targetAccountData->customization = systemIter->customization;
    }

    if (sharing.accessRole == api::SystemAccessRole::none)
    {
        //< Removing sharing. No sense to create account
        result = m_accountManager->fetchExistingAccountByEmail(
            connection,
            sharing.accountEmail,
            targetAccountData);
    }
    else
    {
        result = m_accountManager->fetchExistingAccountOrCreateNewOneByEmail(
            connection,
            sharing.accountEmail,
            targetAccountData);
    }

    if (result != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error fetching account by email %1. %2")
            .arg(sharing.accountEmail).arg(QnLexical::serialized(result)),
            cl_logDEBUG1);
        return result;
    }

    QSqlQuery updateRemoveSharingQuery(*connection);
    if (sharing.accessRole == api::SystemAccessRole::none)
        updateRemoveSharingQuery.prepare(
            "DELETE FROM system_to_account "
            "  WHERE account_id=:accountID AND system_id=:systemID");
    else
        updateRemoveSharingQuery.prepare(
            "REPLACE INTO system_to_account( "
                "account_id, system_id, access_role_id, "
                "group_id, custom_permissions, is_enabled, vms_user_id ) "
            "VALUES( :accountID, :systemID, :accessRole, "
                    ":groupID, :customPermissions, :isEnabled, :vmsUserId )");
    QnSql::bind(sharing, &updateRemoveSharingQuery);
    updateRemoveSharingQuery.bindValue(
        ":accountID",
        QnSql::serialized_field(targetAccountData->id));
    if (!updateRemoveSharingQuery.exec())
    {
        NX_LOG(lm("Failed to update/remove sharing. system %1, account %2, access role %3. %4")
            .arg(sharing.systemID).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(sharing.accessRole))
            .arg(updateRemoveSharingQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::updateSharingInDbAndGenerateTransaction(
    QSqlDatabase* const connection,
    const data::SystemSharing& sharing)
{
    data::AccountData account;
    nx::db::DBResult result = updateSharingInDb(connection, sharing, &account);
    if (result != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error sharing/unsharing system %1 with account %2. %3")
            .arg(sharing.systemID).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(result)),
            cl_logDEBUG1);
        return result;
    }

    if (sharing.accessRole != api::SystemAccessRole::none)
    {
        //generating saveUser transaction
        ::ec2::ApiUserData userData;
        ec2::convert(sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(account.fullName);
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::saveUser>(
                connection,
                sharing.systemID.c_str(),
                std::move(userData));

        //generating "save full name" transaction
        ::ec2::ApiResourceParamWithRefData fullNameData;
        fullNameData.resourceId = QnUuid(sharing.vmsUserId.c_str());
        fullNameData.name = Qn::USER_FULL_NAME;
        fullNameData.value = QString::fromStdString(account.fullName);
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::setResourceParam>(
                connection,
                sharing.systemID.c_str(),
                std::move(fullNameData));
    }
    else
    {
        //generating removeUser transaction
        ::ec2::ApiIdData userId;
        ec2::convert(sharing, &userId);
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::removeUser>(
                connection,
                sharing.systemID.c_str(),
                std::move(userId));

        //generating removeResourceParam transaction
        ::ec2::ApiResourceParamWithRefData fullNameParam;
        fullNameParam.resourceId = QnUuid(sharing.vmsUserId.c_str());
        fullNameParam.name = Qn::USER_FULL_NAME;
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::removeResourceParam>(
                connection,
                sharing.systemID.c_str(),
                std::move(fullNameParam));
    }

    return result;
}

void SystemManager::sharingUpdated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //updating "systems by account id" index
        QnMutexLocker lk(&m_mutex);
        auto& accountSystemPairIndex =
            m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
        accountSystemPairIndex.erase(sharing);
        if (sharing.accessRole != api::SystemAccessRole::none)
        {
            //inserting modified version
            accountSystemPairIndex.emplace(createDerivedFromBase(std::move(sharing)));
        }

        return completionHandler(api::ResultCode::ok);
    }

    completionHandler(
        dbResult == nx::db::DBResult::notFound
        ? api::ResultCode::notFound
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::updateSystemNameInDB(
    QSqlDatabase* const connection,
    const data::SystemNameUpdate& data)
{
    const auto result = execSystemNameUpdate(connection, data);
    if (result != db::DBResult::ok)
        return result;

    // Generating transaction.
    ::ec2::ApiResourceParamWithRefData systemNameData;
    systemNameData.resourceId = QnUserResource::kAdminGuid;
    systemNameData.name = QnGlobalSettings::kNameSystemName;
    systemNameData.value = QString::fromStdString(data.name);

    return m_transactionLog->generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            connection,
            data.systemID.c_str(),
            std::move(systemNameData));
}

nx::db::DBResult SystemManager::execSystemNameUpdate(
    QSqlDatabase* const connection,
    const data::SystemNameUpdate& data)
{
    QSqlQuery updateSystemNameQuery(*connection);
    updateSystemNameQuery.prepare(
        "UPDATE system "
        "SET name=:name "
        "WHERE id=:systemID");
    QnSql::bind(data, &updateSystemNameQuery);
    if (!updateSystemNameQuery.exec())
    {
        NX_LOGX(lm("Failed to update system %1 name in DB to %2. %3")
            .arg(data.systemID).arg(data.name)
            .arg(updateSystemNameQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }
    return db::DBResult::ok;
}

void SystemManager::updateSystemNameInCache(
    data::SystemNameUpdate data)
{
    //updating system name in cache
    QnMutexLocker lk(&m_mutex);

    auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    auto systemIter = systemByIdIndex.find(data.systemID);
    if (systemIter != systemByIdIndex.end())
    {
        systemByIdIndex.modify(
            systemIter,
            [&data](data::SystemData& system) { system.name = std::move(data.name); });
    }
}

void SystemManager::systemNameUpdated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemNameUpdate data,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSystemNameInCache(std::move(data));
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

nx::db::DBResult SystemManager::activateSystem(
    QSqlDatabase* const connection,
    const std::string& systemID)
{
    QSqlQuery updateSystemStatusQuery(*connection);
    updateSystemStatusQuery.prepare(
        "UPDATE system "
        "SET status_code=:statusCode, expiration_utc_timestamp=:expirationTimeUtc "
        "WHERE id=:id");
    updateSystemStatusQuery.bindValue(
        ":statusCode",
        QnSql::serialized_field(static_cast<int>(api::SystemStatus::ssActivated)));
    updateSystemStatusQuery.bindValue(
        ":id",
        QnSql::serialized_field(systemID));
    updateSystemStatusQuery.bindValue(
        ":expirationTimeUtc",
        std::numeric_limits<int>::max());
    if (!updateSystemStatusQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(updateSystemStatusQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void SystemManager::systemActivated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    std::string systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);

        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        auto systemIter = systemByIdIndex.find(systemId);
        if (systemIter != systemByIdIndex.end())
        {
            systemByIdIndex.modify(
                systemIter,
                [](data::SystemData& system) {
                    system.status = api::SystemStatus::ssActivated;
                    system.expirationTimeUtc = std::numeric_limits<int>::max();
                });
        }
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

constexpr const int kSecondsPerDay = 60*60*24;

nx::db::DBResult SystemManager::updateSystemAccessWeightInDb(
    QSqlDatabase* dbConnection,
    const data::UserSessionDescriptor& userSessionDescriptor,
    double* const systemAccessWeight)
{
    NX_ASSERT(userSessionDescriptor.accountEmail && userSessionDescriptor.systemId);

    QSqlQuery selectUsageStatisticsQuery(*dbConnection);
    selectUsageStatisticsQuery.setForwardOnly(true);
    selectUsageStatisticsQuery.prepare(
        R"sql(
        SELECT sa.account_id, sa.last_login_time_utc, sa.usage_frequency 
        FROM system_to_account sa, account a
        WHERE sa.account_id=a.id AND a.email=:account_email AND sa.system_id=:system_id
        )sql");
    selectUsageStatisticsQuery.bindValue(
        ":account_email",
        QnSql::serialized_field(userSessionDescriptor.accountEmail.get()));
    selectUsageStatisticsQuery.bindValue(
        ":system_id",
        QnSql::serialized_field(userSessionDescriptor.systemId.get()));
    if (!selectUsageStatisticsQuery.exec())
    {
        NX_LOGX(lm("Error executing request to select system %1 usage by %2. %3")
            .arg(*userSessionDescriptor.systemId)
            .arg(*userSessionDescriptor.accountEmail)
            .arg(selectUsageStatisticsQuery.lastError().text()), cl_logWARNING);
        return nx::db::DBResult::ioError;
    }
    if (!selectUsageStatisticsQuery.next())
    {
        // User could be removed.
        return nx::db::DBResult::notFound;
    }

    const auto lastLoginTimeUtc = 
        selectUsageStatisticsQuery.value("last_login_time_utc").toLongLong();
    const auto currentUsageFrequency =
        selectUsageStatisticsQuery.value("usage_frequency").toFloat();
    const auto accountId = selectUsageStatisticsQuery.value("account_id").toString();

    using namespace std::chrono;
    const qint64 currentTimeUtc = 
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    const auto fullDaysSinceLastLogin = 
        (currentTimeUtc - lastLoginTimeUtc) / kSecondsPerDay;
    *systemAccessWeight = std::max<float>(
        (1 - fullDaysSinceLastLogin/30.0) * currentUsageFrequency,
        0);
    const auto newUsageFrequency = *systemAccessWeight + 1;

    QSqlQuery updateUsageStatisticsQuery(*dbConnection);
    updateUsageStatisticsQuery.prepare(
        R"sql(
        UPDATE system_to_account
        SET last_login_time_utc=:last_login_time_utc, usage_frequency=:usage_frequency
        WHERE account_id=:account_id AND system_id=:system_id
        )sql");
    updateUsageStatisticsQuery.bindValue(":last_login_time_utc", currentTimeUtc);
    updateUsageStatisticsQuery.bindValue(":usage_frequency", newUsageFrequency);
    updateUsageStatisticsQuery.bindValue(":account_id", accountId);
    updateUsageStatisticsQuery.bindValue(
        ":system_id",
        QnSql::serialized_field(userSessionDescriptor.systemId.get()));
    if (!updateUsageStatisticsQuery.exec())
    {
        NX_LOGX(lm("Error executing request to update system %1 usage by %2 in db. %3")
            .arg(*userSessionDescriptor.systemId)
            .arg(*userSessionDescriptor.accountEmail)
            .arg(updateUsageStatisticsQuery.lastError().text()), cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAccessWeightUpdatedInDb(
    QnCounter::ScopedIncrement asyncCallLocker,
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::UserSessionDescriptor userSessionDescriptor,
    double systemAccessWeight,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        // Updating in cache.
        QnMutexLocker lk(&m_mutex);

        auto& bySystemIdAndAccountEmailIndex = 
            m_accountAccessRoleForSystem.get<kSharingBySystemIdAndAccountEmailIndex>();
        auto systemIter = bySystemIdAndAccountEmailIndex.find(
            std::make_pair(
                userSessionDescriptor.systemId.get(),
                userSessionDescriptor.accountEmail.get()));
        if (systemIter != bySystemIdAndAccountEmailIndex.end())
        {
            bySystemIdAndAccountEmailIndex.modify(
                systemIter,
                [systemAccessWeight](api::SystemSharingEx& systemSharing)
                {
                    systemSharing.systemAccessWeight = systemAccessWeight;
                });
        }
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
        case api::SystemAccessRole::localAdmin:
            resultData.accessRoles.emplace_back(api::SystemAccessRole::liveViewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::viewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::advancedViewer);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::localAdmin);
            resultData.accessRoles.emplace_back(api::SystemAccessRole::cloudAdmin);
            break;

        case api::SystemAccessRole::liveViewer:
        case api::SystemAccessRole::viewer:
        case api::SystemAccessRole::advancedViewer:
        case api::SystemAccessRole::disabled:
        case api::SystemAccessRole::custom:
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
            [&cacheFilledPromise](
                QSqlDatabase* /*connection*/,
                db::DBResult dbResult,
                int /*dummy*/)
            {
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
            [&cacheFilledPromise](
                QSqlDatabase* /*connection*/,
                db::DBResult dbResult,
                int /*dummy*/)
            {
                cacheFilledPromise.set_value(dbResult);
            });
        //waiting for completion
        const auto result = future.get();
        if (result != db::DBResult::ok)
            return result;
    }

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchSystems(QSqlDatabase* connection, int* const /*dummy*/)
{
    QSqlQuery readSystemsQuery(*connection);
    readSystemsQuery.setForwardOnly(true);
    readSystemsQuery.prepare(
        "SELECT s.id, s.name, s.customization, s.auth_key as authKey, "
        "       a.email as ownerAccountEmail, s.status_code as status, "
        "       s.expiration_utc_timestamp as expirationTimeUtc, s.seq as systemSequence "
        "FROM system s, account a "
        "WHERE s.owner_account_id = a.id");
    if (!readSystemsQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(readSystemsQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }
    
    std::vector<data::SystemData> systems;
    QnSql::fetch_many(readSystemsQuery, &systems);

    for (auto& system : systems)
    {
        auto idCopy = system.id;
        if (!m_systems.insert(std::move(system)).second)
        {
            NX_ASSERT(false);
        }
    }

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchSystemToAccountBinder(
    QSqlDatabase* connection,
    int* const /*dummy*/)
{
    QSqlQuery readSystemToAccountQuery(*connection);
    readSystemToAccountQuery.setForwardOnly(true);
    readSystemToAccountQuery.prepare(
        "SELECT a.email as accountEmail, "
               "sa.system_id as systemID, "
               "sa.access_role_id as accessRole, "
               "sa.group_id as groupID, "
               "sa.custom_permissions as customPermissions, "
               "sa.is_enabled as isEnabled, "
               "sa.vms_user_id as vmsUserId "
        "FROM system_to_account sa, account a "
        "WHERE sa.account_id = a.id");
    if (!readSystemToAccountQuery.exec())
    {
        NX_LOG(lit("Failed to read system list from DB. %1").
            arg(readSystemToAccountQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<data::SystemSharing> systemToAccount;
    QnSql::fetch_many(readSystemToAccountQuery, &systemToAccount);
    for (auto& sharing: systemToAccount)
        m_accountAccessRoleForSystem.emplace(createDerivedFromBase(std::move(sharing)));

    return db::DBResult::ok;
}

void SystemManager::dropExpiredSystems(uint64_t /*timerId*/)
{
    bool expected = false;
    if (!m_dropExpiredSystemsTaskStillRunning.compare_exchange_strong(expected, true))
    {
        NX_LOGX(lit("Previous 'drop expired systems' task is still running..."),
            cl_logWARNING);
        return; //previous task is still running
    }

    using namespace std::placeholders;
    m_dbManager->executeUpdate(
        std::bind(&SystemManager::deleteExpiredSystemsFromDb, this, _1),
        std::bind(&SystemManager::expiredSystemsDeletedFromDb, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2));
}

nx::db::DBResult SystemManager::deleteExpiredSystemsFromDb(QSqlDatabase* connection)
{
    //dropping expired not-activated systems and expired marked-for-removal systems
    QSqlQuery dropExpiredSystems(*connection);
    dropExpiredSystems.prepare(
        "DELETE FROM system "
        "WHERE (status_code=:notActivatedStatusCode OR status_code=:deletedStatusCode) "
            "AND expiration_utc_timestamp < :currentTime");
    dropExpiredSystems.bindValue(
        ":notActivatedStatusCode",
        static_cast<int>(api::SystemStatus::ssNotActivated));
    dropExpiredSystems.bindValue(
        ":deletedStatusCode",
        static_cast<int>(api::SystemStatus::ssDeleted));
    dropExpiredSystems.bindValue(
        ":currentTime",
        (int)::time(NULL));
    if (!dropExpiredSystems.exec())
    {
        NX_LOGX(lit("Error deleting expired systems from DB. %1").
            arg(dropExpiredSystems.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void SystemManager::expiredSystemsDeletedFromDb(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    QSqlDatabase* /*connection*/,
    nx::db::DBResult dbResult)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //cleaning up systems cache
        QnMutexLocker lk(&m_mutex);
        auto& systemsByExpirationTime = m_systems.get<kSystemByExpirationTimeIndex>();
        for (auto systemIter = systemsByExpirationTime.lower_bound(::time(NULL));
             systemIter != systemsByExpirationTime.end();
             )
        {
            NX_ASSERT(systemIter->status != api::SystemStatus::ssActivated);

            auto& sharingBySystemId = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
            sharingBySystemId.erase(systemIter->id);

            systemIter = systemsByExpirationTime.erase(systemIter);
        }
    }

    m_dropExpiredSystemsTaskStillRunning = false;
}

nx::db::DBResult SystemManager::processEc2SaveUser(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::ApiUserData data,
    data::SystemSharing* const systemSharingData)
{
    NX_LOGX(lm("Processing vms transaction saveUser. "
        "user %1, systemId %2, permissions %3, enabled %4")
        .arg(data.email).arg(systemId)
        .arg(QnLexical::serialized(data.permissions)).arg(data.isEnabled),
        cl_logDEBUG2);

    // Preparing SystemSharing structure.
    systemSharingData->systemID = systemId.toStdString();
    ec2::convert(data, systemSharingData);

    // Updating db.
    data::AccountData account;
    const nx::db::DBResult result = 
        updateSharingInDb(dbConnection, *systemSharingData, &account);
    if (result != nx::db::DBResult::ok)
        return result;

    // Generating "save full name" transaction.
    ::ec2::ApiResourceParamWithRefData fullNameData;
    fullNameData.resourceId = data.id;
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(account.fullName);
    return m_transactionLog->generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            dbConnection,
            systemId,
            std::move(fullNameData));
}

void SystemManager::onEc2SaveUserDone(
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing)
{
    if (dbResult != nx::db::DBResult::ok)
        return;

    //updating "systems by account id" index
    QnMutexLocker lk(&m_mutex);
    auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    accountSystemPairIndex.erase(sharing);
    if (sharing.accessRole != api::SystemAccessRole::none)
    {
        //inserting modified version
        accountSystemPairIndex.emplace(createDerivedFromBase(std::move(sharing)));
    }
}

nx::db::DBResult SystemManager::processEc2RemoveUser(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::ApiIdData data,
    data::SystemSharing* const systemSharingData)
{
    NX_LOGX(
        QnLog::EC2_TRAN_LOG,
        lm("Processing vms transaction removeUser. systemId %1, vms user id %2")
            .arg(systemId).str(data.id),
        cl_logDEBUG2);

    systemSharingData->systemID = systemId.toStdString();
    systemSharingData->vmsUserId = data.id.toSimpleString().toStdString();

    QSqlQuery removeSharingQuery(*dbConnection);
    removeSharingQuery.prepare(
        "DELETE FROM system_to_account "
        "  WHERE system_id=:systemId AND vms_user_id=:vmsUserId");
    removeSharingQuery.bindValue(
        ":systemId",
        QnSql::serialized_field(systemId));
    removeSharingQuery.bindValue(
        ":vmsUserId",
        QnSql::serialized_field(systemSharingData->vmsUserId));
    if (!removeSharingQuery.exec())
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Failed to remove sharing by vms user id. system %1, vms user id %2. %3")
                .arg(systemId).str(data.id).arg(dbConnection->lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void SystemManager::onEc2RemoveUserDone(
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing)
{
    if (dbResult != nx::db::DBResult::ok)
        return;

    //updating "systems by account id" index
    QnMutexLocker lk(&m_mutex);
    auto& systemSharingBySystemIdAndVmsUserId =
        m_accountAccessRoleForSystem.get<kSharingBySystemIdAndVmsUserIdIndex>();
    systemSharingBySystemIdAndVmsUserId.erase(
        std::make_pair(sharing.systemID, sharing.vmsUserId));
}

nx::db::DBResult SystemManager::processSetResourceParam(
    QSqlDatabase* dbConnection,
    const nx::String& systemId,
    ::ec2::ApiResourceParamWithRefData data,
    data::SystemNameUpdate* const systemNameUpdate)
{
    if (data.resourceId != QnUserResource::kAdminGuid ||
        data.name != QnGlobalSettings::kNameSystemName)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Ignoring transaction setResourceParam with. "
               "systemId %1, resourceId %2, param name %3, param value %4")
                .arg(systemId).arg(data.resourceId).arg(data.name).arg(data.value),
            cl_logDEBUG1);
        return nx::db::DBResult::ok;
    }

    systemNameUpdate->systemID = systemId.toStdString();
    systemNameUpdate->name = data.value.toStdString();
    return execSystemNameUpdate(dbConnection, *systemNameUpdate);
}

void SystemManager::onEc2SetResourceParamDone(
    QSqlDatabase* /*dbConnection*/,
    nx::db::DBResult dbResult,
    data::SystemNameUpdate systemNameUpdate)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSystemNameInCache(std::move(systemNameUpdate));
    }
}

}   //cdb
}   //nx
