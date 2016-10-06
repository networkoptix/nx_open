#include "system_manager.h"

#include <limits>

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_resource_data.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

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

// TODO: #ak should get rid of following function
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
        if (systemIter->expirationTimeUtc > nx::utils::timeSinceEpoch().count() ||
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
            nx::db::QueryContext* const queryContext) -> nx::db::DBResult
        {
            return insertSystemToDB(
                queryContext,
                *registrationDataWithAccountPtr,
                newSystemDataPtr);
        };

    auto onDbUpdateCompletedFunc = 
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            registrationDataWithAccount = std::move(registrationDataWithAccount),
            newSystemData = std::move(newSystemData),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* queryContext,
                nx::db::DBResult dbResult)
        {
            systemAdded(
                std::move(locker),
                queryContext,
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
                // Calculating system weight.
                systemDataEx.usageFrequency = calculateSystemUsageFrequency(
                    sharingData->lastLoginTime,
                    sharingData->usageFrequency + 1);
                systemDataEx.lastLoginTime = sharingData->lastLoginTime;
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
    const AuthorizationInfo& authzInfo,
    data::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;

    std::string grantorEmail;
    if (!authzInfo.get(attr::authAccountEmail, &grantorEmail))
    {
        NX_ASSERT(false);
        return completionHandler(api::ResultCode::forbidden);
    }

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
        [this, grantorEmail = std::move(grantorEmail), sharingOHeapPtr](
            nx::db::QueryContext* const queryContext) -> nx::db::DBResult
        {
            return updateSharingInDbAndGenerateTransaction(
                queryContext,
                grantorEmail,
                *sharingOHeapPtr);
        };

    auto onDbUpdateCompletedFunc = 
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            sharingOHeap = std::move(sharingOHeap),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* queryContext,
                nx::db::DBResult dbResult)
        {
            sharingUpdated(
                std::move(locker),
                queryContext,
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

void SystemManager::renameSystem(
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
            nx::db::QueryContext* const queryContext) -> nx::db::DBResult
    {
        return updateSystemNameInDB(
            queryContext,
            *systemNameDataOnHeapPtr);
    };

    auto onDbUpdateCompletedFunc =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            systemNameData = std::move(systemNameDataOnHeap),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* queryContext,
                nx::db::DBResult dbResult) mutable
        {
            systemNameUpdated(
                std::move(locker),
                queryContext,
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
    m_dbManager->executeUpdate<data::UserSessionDescriptor, SaveUserSessionResult>(
        std::bind(&SystemManager::saveUserSessionStartToDb, this, _1, _2, _3),
        std::move(userSessionDescriptor),
        std::bind(&SystemManager::userSessionStartSavedToDb, this,
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
    nx::db::QueryContext* const queryContext,
    const data::SystemRegistrationDataWithAccount& newSystem,
    InsertNewSystemToDbResult* const result)
{
    nx::db::DBResult dbResult = nx::db::DBResult::ok;

    dbResult = insertNewSystemDataToDb(queryContext, newSystem, result);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    dbResult = m_transactionLog->updateTimestampHiForSystem(
        queryContext,
        result->systemData.id.c_str(),
        result->systemData.systemSequence);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    dbResult = insertOwnerSharingToDb(
        queryContext, result->systemData.id, newSystem, &result->ownerSharing);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::insertNewSystemDataToDb(
    nx::db::QueryContext* const queryContext,
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

    QSqlQuery insertSystemQuery(*queryContext->connection());
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
        nx::utils::timeSinceEpoch().count() +
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
    QSqlQuery selectSystemSequence(*queryContext->connection());
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
    nx::db::QueryContext* const queryContext,
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
    const auto resultCode = updateSharingInDbAndGenerateTransaction(
        queryContext,
        newSystem.accountEmail,
        *ownerSharing);
    if (resultCode != nx::db::DBResult::ok)
    {
        NX_LOG(lm("Could not insert system %1 to account %2 binding into DB. %3").
            arg(newSystem.name).arg(newSystem.accountEmail).
            arg(queryContext->connection()->lastError().text()),
            cl_logDEBUG1);
        return resultCode;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
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
    nx::db::QueryContext* /*queryContext*/,
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
    nx::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    //marking system as "deleted"
    QSqlQuery markSystemAsRemoved(*queryContext->connection());
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
        (int)(nx::utils::timeSinceEpoch().count() +
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
    const auto dbResult = deleteSharing(
        queryContext,
        systemId);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOG(lm("Could not delete system %1 from system_to_account. %2").
            arg(systemId).arg(queryContext->connection()->lastError().text()),
            cl_logDEBUG1);
        return dbResult;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemMarkedAsDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
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
                        nx::utils::timeSinceEpoch().count() +
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
    nx::db::QueryContext* const queryContext,
    const data::SystemID& systemID)
{
    const auto dbResult = deleteSharing(queryContext, systemID.systemID);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOG(lm("Could not delete system %1 from system_to_account. %2")
            .arg(systemID.systemID).arg(queryContext->connection()->lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    QSqlQuery removeSystem(*queryContext->connection());
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
    nx::db::QueryContext* /*queryContext*/,
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
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    data::AccountData* const targetAccountData)
{
    nx::db::DBResult result = fetchAccountToShareWith(
        queryContext,
        grantorEmail,
        sharing,
        targetAccountData);
    if (result != nx::db::DBResult::ok)
        return result;

    api::SystemSharingEx resultSharing = createDerivedFromBase(sharing);
    resultSharing.lastLoginTime = std::chrono::system_clock::now();
    resultSharing.accountID = targetAccountData->id;

    if (sharing.accessRole == api::SystemAccessRole::none)
    {
        return deleteSharing(
            queryContext,
            resultSharing.systemID,
            std::array<SqlFilterByField, 1>(
                {{"account_id", ":accountID", QnSql::serialized_field(targetAccountData->id)}}));
    }

    result = calculateUsageFrequencyForANewSystem(
        queryContext,
        targetAccountData->id,
        resultSharing.systemID,
        &resultSharing.usageFrequency);
    if (result != nx::db::DBResult::ok)
        return result;

    QSqlQuery updateRemoveSharingQuery(*queryContext->connection());
    updateRemoveSharingQuery.prepare(
        R"sql(
        REPLACE INTO system_to_account(
            account_id, system_id, access_role_id, group_id, custom_permissions,
            is_enabled, vms_user_id, last_login_time_utc, usage_frequency)
        VALUES(:accountID, :systemID, :accessRole, :groupID, :customPermissions,
                :isEnabled, :vmsUserId, :lastLoginTime, :usageFrequency)
        )sql");
    QnSql::bind(resultSharing, &updateRemoveSharingQuery);
    if (!updateRemoveSharingQuery.exec())
    {
        NX_LOG(lm("Failed to update/remove sharing. system %1, account %2, access role %3. %4")
            .arg(sharing.systemID).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(sharing.accessRole))
            .arg(updateRemoveSharingQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    queryContext->transaction()->addAfterCommitHandler(
        [this, resultSharing = std::move(resultSharing)](
            nx::db::DBResult dbResult) mutable
        {
            if (dbResult == nx::db::DBResult::ok)
                updateSharingInCache(std::move(resultSharing));
        });

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchUserSharing(
    nx::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    const std::string& systemId,
    api::SystemSharingEx* const sharing)
{
    std::array<SqlFilterByField, 2> sqlFilter = 
        {{{"email", ":accountEmail", QnSql::serialized_field(accountEmail)},
          {"system_id", ":systemId", QnSql::serialized_field(systemId)}}};

    std::vector<api::SystemSharingEx> sharings;
    const auto result = fetchUserSharings(
        queryContext,
        &sharings,
        std::move(sqlFilter));
    if (result != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error fetching sharing of system %1 to account %2")
            .arg(systemId).arg(accountEmail),
            cl_logWARNING);
        return result;
    }
    if (sharings.empty())
        return nx::db::DBResult::notFound;

    NX_ASSERT(sharings.size() == 1);
    *sharing = std::move(sharings[0]);

    return nx::db::DBResult::ok;
}

template<std::size_t filterFieldCount>
nx::db::DBResult SystemManager::fetchUserSharings(
    nx::db::QueryContext* const queryContext,
    std::vector<api::SystemSharingEx>* const sharings,
    std::array<SqlFilterByField, filterFieldCount> filterFields)
{
    QString sqlRequestStr = 
        R"sql(
        SELECT a.id as accountID,
               a.email as accountEmail,
               sa.system_id as systemID,
               sa.access_role_id as accessRole,
               sa.group_id as groupID,
               sa.custom_permissions as customPermissions,
               sa.is_enabled as isEnabled,
               sa.vms_user_id as vmsUserId,
               sa.last_login_time_utc as lastLoginTime,
               sa.usage_frequency as usageFrequency
        FROM system_to_account sa, account a
        WHERE sa.account_id=a.id
        )sql";
    QString filterStr;
    for (const auto& filterField : filterFields)
    {
        filterStr += lm(" AND %1=%2")
            .arg(filterField.fieldName).arg(filterField.bindValueName);
    }
    sqlRequestStr += filterStr;

    QSqlQuery selectSharingQuery(*queryContext->connection());
    selectSharingQuery.setForwardOnly(true);
    selectSharingQuery.prepare(sqlRequestStr);
    for (const auto& filterField : filterFields)
    {
        selectSharingQuery.bindValue(
            filterField.bindValueName,
            filterField.fieldValue);
    }
    if (!selectSharingQuery.exec())
    {
        NX_LOGX(lm("Error executing request to select sharings with filter %1. %2")
            .arg(filterStr).arg(selectSharingQuery.lastError().text()),
            cl_logWARNING);
        return nx::db::DBResult::ioError;
    }
    QnSql::fetch_many(selectSharingQuery, sharings);

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchAccountToShareWith(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    data::AccountData* const targetAccountData)
{
    nx::db::DBResult result = nx::db::DBResult::ok;

    std::string systemName;
    {
        // Initializing account customization for case if account has to be created.
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        const auto systemIter = systemByIdIndex.find(sharing.systemID);
        if (systemIter != systemByIdIndex.end())
        {
            targetAccountData->customization = systemIter->customization;
            systemName = systemIter->name;
        }
    }

    if (sharing.accessRole == api::SystemAccessRole::none)
    {
        //< Removing sharing. No sense to create account
        result = m_accountManager->fetchExistingAccountByEmail(
            queryContext,
            sharing.accountEmail,
            targetAccountData);
    }
    else
    {
        const auto grantorAccountData = m_accountManager->findAccountByUserName(grantorEmail);

        auto notification = std::make_unique<InviteUserNotification>();
        notification->message.sharer_email = grantorEmail;
        if (grantorAccountData)
            notification->message.sharer_name = grantorAccountData->fullName;
        notification->message.system_id = sharing.systemID;
        notification->message.system_name = systemName;
        result = m_accountManager->fetchExistingAccountOrCreateNewOneByEmail(
            queryContext,
            sharing.accountEmail,
            targetAccountData,
            std::move(notification));
    }

    if (result != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error fetching account by email %1. %2")
            .arg(sharing.accountEmail).arg(QnLexical::serialized(result)),
            cl_logDEBUG1);
        return result;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::calculateUsageFrequencyForANewSystem(
    nx::db::QueryContext* const queryContext,
    const std::string& accountId,
    const std::string& systemId,
    float* const newSystemInitialUsageFrequency)
{
    QSqlQuery calculateUsageFrequencyForTheNewSystem(*queryContext->connection());
    calculateUsageFrequencyForTheNewSystem.setForwardOnly(true);
    calculateUsageFrequencyForTheNewSystem.prepare(
        R"sql(
            SELECT MAX(usage_frequency) + 1 
            FROM system_to_account 
            WHERE account_id = :accountID
            )sql");
    calculateUsageFrequencyForTheNewSystem.bindValue(
        ":accountID",
        QnSql::serialized_field(accountId));
    if (!calculateUsageFrequencyForTheNewSystem.exec() ||
        !calculateUsageFrequencyForTheNewSystem.next())
    {
        NX_LOGX(lm("Failed to fetch usage_frequency for the new system %1 of account %2. %3")
            .arg(systemId).arg(accountId)
            .arg(calculateUsageFrequencyForTheNewSystem.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    *newSystemInitialUsageFrequency = 
        calculateUsageFrequencyForTheNewSystem.value(0).toFloat();
    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::updateSharingInDbAndGenerateTransaction(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing)
{
    data::AccountData account;
    nx::db::DBResult result = updateSharingInDb(queryContext, grantorEmail, sharing, &account);
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
                queryContext,
                sharing.systemID.c_str(),
                std::move(userData));

        //generating "save full name" transaction
        ::ec2::ApiResourceParamWithRefData fullNameData;
        fullNameData.resourceId = QnUuid(sharing.vmsUserId.c_str());
        fullNameData.name = Qn::USER_FULL_NAME;
        fullNameData.value = QString::fromStdString(account.fullName);
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::setResourceParam>(
                queryContext,
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
                queryContext,
                sharing.systemID.c_str(),
                std::move(userId));

        //generating removeResourceParam transaction
        ::ec2::ApiResourceParamWithRefData fullNameParam;
        fullNameParam.resourceId = QnUuid(sharing.vmsUserId.c_str());
        fullNameParam.name = Qn::USER_FULL_NAME;
        result = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::removeResourceParam>(
                queryContext,
                sharing.systemID.c_str(),
                std::move(fullNameParam));
    }

    return result;
}

void SystemManager::sharingUpdated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSharingInCache(std::move(sharing));
        return completionHandler(api::ResultCode::ok);
    }

    completionHandler(
        dbResult == nx::db::DBResult::notFound
        ? api::ResultCode::notFound
        : api::ResultCode::dbError);
}

void SystemManager::updateSharingInCache(
    data::SystemSharing sharing)
{
    updateSharingInCache(
        createDerivedFromBase(std::move(sharing)),
        false);
}

void SystemManager::updateSharingInCache(
    api::SystemSharingEx sharing,
    bool updateExFields)
{
    // Updating "systems by account id" index.
    QnMutexLocker lk(&m_mutex);
    auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    if (sharing.accessRole == api::SystemAccessRole::none)
    {
        accountSystemPairIndex.erase(sharing);
        return;
    }

    const auto iterAndInsertionFlag = accountSystemPairIndex.insert(sharing);
    if (iterAndInsertionFlag.second)
        return;

    // Updating existing data while preserving fields of SystemSharingEx structure.
    accountSystemPairIndex.modify(
        iterAndInsertionFlag.first,
        [&sharing, updateExFields](api::SystemSharingEx& existingData) mutable
        {
            if (updateExFields)
                existingData = std::move(sharing);
            else
                existingData.api::SystemSharing::operator=(sharing);
        });
}

nx::db::DBResult SystemManager::updateSystemNameInDB(
    nx::db::QueryContext* const queryContext,
    const data::SystemNameUpdate& data)
{
    const auto result = execSystemNameUpdate(queryContext, data);
    if (result != db::DBResult::ok)
        return result;

    // Generating transaction.
    ::ec2::ApiResourceParamWithRefData systemNameData;
    systemNameData.resourceId = QnUserResource::kAdminGuid;
    systemNameData.name = nx::settings_names::kNameSystemName;
    systemNameData.value = QString::fromStdString(data.name);

    return m_transactionLog->generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            queryContext,
            data.systemID.c_str(),
            std::move(systemNameData));
}

nx::db::DBResult SystemManager::execSystemNameUpdate(
    nx::db::QueryContext* const queryContext,
    const data::SystemNameUpdate& data)
{
    QSqlQuery updateSystemNameQuery(*queryContext->connection());
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
    nx::db::QueryContext* /*queryContext*/,
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
    nx::db::QueryContext* const queryContext,
    const std::string& systemID)
{
    QSqlQuery updateSystemStatusQuery(*queryContext->connection());
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
    nx::db::QueryContext* /*queryContext*/,
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

nx::db::DBResult SystemManager::saveUserSessionStartToDb(
    nx::db::QueryContext* queryContext,
    const data::UserSessionDescriptor& userSessionDescriptor,
    SaveUserSessionResult* const result)
{
    using namespace std::chrono;

    NX_ASSERT(userSessionDescriptor.accountEmail && userSessionDescriptor.systemId);

    api::SystemSharingEx sharing;
    auto dbResult = fetchUserSharing(
        queryContext,
        *userSessionDescriptor.accountEmail,
        *userSessionDescriptor.systemId,
        &sharing);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error fetching user sharing (%1, %2). %3")
            .arg(*userSessionDescriptor.accountEmail)
            .arg(*userSessionDescriptor.systemId)
            .arg(QnLexical::serialized(dbResult)),
            cl_logDEBUG1);
        return dbResult;
    }

    // Calculating usage frequency.
    result->lastloginTime = nx::utils::utcTime();
    result->usageFrequency = calculateSystemUsageFrequency(
        sharing.lastLoginTime,
        sharing.usageFrequency);
    const auto newUsageFrequency = result->usageFrequency + 1;

    // Saving new statistics to DB.
    dbResult = updateUserLoginStatistics(
        queryContext,
        sharing.accountID,
        sharing.systemID,
        result->lastloginTime,
        newUsageFrequency);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error updating user login statistics"), cl_logWARNING);
        return dbResult;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::updateUserLoginStatistics(
    nx::db::QueryContext* queryContext,
    const std::string& accountId,
    const std::string& systemId,
    std::chrono::system_clock::time_point lastloginTime,
    float usageFrequency)
{
    QSqlQuery updateUsageStatisticsQuery(*queryContext->connection());
    updateUsageStatisticsQuery.prepare(
        R"sql(
        UPDATE system_to_account
        SET last_login_time_utc=:last_login_time_utc, usage_frequency=:usage_frequency
        WHERE account_id=:account_id AND system_id=:system_id
        )sql");
    updateUsageStatisticsQuery.bindValue(
        ":last_login_time_utc",
        QnSql::serialized_field(lastloginTime));
    updateUsageStatisticsQuery.bindValue(":usage_frequency", usageFrequency);
    updateUsageStatisticsQuery.bindValue(
        ":account_id", QnSql::serialized_field(accountId));
    updateUsageStatisticsQuery.bindValue(
        ":system_id", QnSql::serialized_field(systemId));
    if (!updateUsageStatisticsQuery.exec())
    {
        NX_LOGX(lm("Error executing request to update system %1 usage by %2 in db. %3")
            .arg(systemId).arg(accountId)
            .arg(updateUsageStatisticsQuery.lastError().text()), cl_logWARNING);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::userSessionStartSavedToDb(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::UserSessionDescriptor userSessionDescriptor,
    SaveUserSessionResult result,
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
                [&result](api::SystemSharingEx& systemSharing)
                {
                    systemSharing.usageFrequency = result.usageFrequency;
                    systemSharing.lastLoginTime = result.lastloginTime;
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
                nx::db::QueryContext* /*queryContext*/,
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
                nx::db::QueryContext* /*queryContext*/,
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

nx::db::DBResult SystemManager::fetchSystems(nx::db::QueryContext* queryContext, int* const /*dummy*/)
{
    QSqlQuery readSystemsQuery(*queryContext->connection());
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
    nx::db::QueryContext* queryContext,
    int* const /*dummy*/)
{
    std::vector<api::SystemSharingEx> sharings;
    const auto result = fetchUserSharings(
        queryContext,
        &sharings);
    if (result != nx::db::DBResult::ok)
    {
        NX_LOG(lit("Failed to read system list from DB"), cl_logWARNING);
        return db::DBResult::ioError;
    }

    for (auto& sharing: sharings)
    {
        sharing.usageFrequency -= 1.0;
        m_accountAccessRoleForSystem.emplace(std::move(sharing));
    }

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

nx::db::DBResult SystemManager::deleteExpiredSystemsFromDb(nx::db::QueryContext* queryContext)
{
    //dropping expired not-activated systems and expired marked-for-removal systems
    QSqlQuery dropExpiredSystems(*queryContext->connection());
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
        (int)nx::utils::timeSinceEpoch().count());
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
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        //cleaning up systems cache
        QnMutexLocker lk(&m_mutex);
        auto& systemsByExpirationTime = m_systems.get<kSystemByExpirationTimeIndex>();
        for (auto systemIter = systemsByExpirationTime
                .lower_bound(nx::utils::timeSinceEpoch().count());
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
    nx::db::QueryContext* queryContext,
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

    // We have no information about grantor here. Using system owner...
    std::string ownerEmail;
    {
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        const auto systemIter = systemByIdIndex.find(
            std::string(systemId.constData(), systemId.size()));
        NX_ASSERT(systemIter != systemByIdIndex.end());
        if (systemIter != systemByIdIndex.end())
            ownerEmail = systemIter->ownerAccountEmail;
    }

    // Updating db.
    data::AccountData account;
    const nx::db::DBResult result = 
        updateSharingInDb(queryContext, ownerEmail, *systemSharingData, &account);
    if (result != nx::db::DBResult::ok)
        return result;

    // Generating "save full name" transaction.
    ::ec2::ApiResourceParamWithRefData fullNameData;
    fullNameData.resourceId = data.id;
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(account.fullName);
    return m_transactionLog->generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            queryContext,
            systemId,
            std::move(fullNameData));
}

void SystemManager::onEc2SaveUserDone(
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemSharing sharing)
{
    if (dbResult != nx::db::DBResult::ok)
        return;

    updateSharingInCache(std::move(sharing));
}

nx::db::DBResult SystemManager::processEc2RemoveUser(
    nx::db::QueryContext* queryContext,
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

    const auto dbResult = deleteSharing(
        queryContext,
        systemId.toStdString(),
        std::array<SqlFilterByField, 1>(
            {{"vms_user_id", ":vmsUserId",
                QnSql::serialized_field(systemSharingData->vmsUserId)}}));
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Failed to remove sharing by vms user id. system %1, vms user id %2. %3")
                .arg(systemId).str(data.id)
                .arg(queryContext->connection()->lastError().text()),
            cl_logDEBUG1);
        return dbResult;
    }

    return db::DBResult::ok;
}

template<std::size_t filterFieldCount>
nx::db::DBResult SystemManager::deleteSharing(
    nx::db::QueryContext* queryContext,
    const std::string& systemId,
    std::array<SqlFilterByField, filterFieldCount> filterFields)
{
    QSqlQuery removeSharingQuery(*queryContext->connection());
    QString sqlQueryStr = 
        R"sql(
        DELETE FROM system_to_account WHERE system_id=:systemId
        )sql";
    QString filterStr;
    for (const auto& filterField: filterFields)
    {
        filterStr += lm(" AND %1=%2")
            .arg(filterField.fieldName).arg(filterField.bindValueName);
    }
    sqlQueryStr += filterStr;
    removeSharingQuery.prepare(sqlQueryStr);
    removeSharingQuery.bindValue(
        ":systemId",
        QnSql::serialized_field(systemId));
    for (const auto& filterField: filterFields)
    {
        removeSharingQuery.bindValue(
            filterField.bindValueName,
            filterField.fieldValue);
    }
    if (!removeSharingQuery.exec())
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Failed to remove sharing. system %1, filter \"%2\". %3")
            .arg(systemId).str(filterStr).arg(removeSharingQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }
    return db::DBResult::ok;
}

void SystemManager::onEc2RemoveUserDone(
    nx::db::QueryContext* /*queryContext*/,
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
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    ::ec2::ApiResourceParamWithRefData data,
    data::SystemNameUpdate* const systemNameUpdate)
{
    if (data.resourceId != QnUserResource::kAdminGuid ||
        data.name != nx::settings_names::kNameSystemName)
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
    return execSystemNameUpdate(queryContext, *systemNameUpdate);
}

void SystemManager::onEc2SetResourceParamDone(
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemNameUpdate systemNameUpdate)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSystemNameInCache(std::move(systemNameUpdate));
    }
}

float SystemManager::calculateSystemUsageFrequency(
    std::chrono::system_clock::time_point lastLoginTime,
    float currentUsageFrequency)
{
    const auto fullDaysSinceLastLogin =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::utcTime() - lastLoginTime).count() / kSecondsPerDay;
    return std::max<float>(
        (1 - fullDaysSinceLastLogin / 30.0) * currentUsageFrequency,
        0);
}

}   //cdb
}   //nx
