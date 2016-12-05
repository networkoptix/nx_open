#include "system_manager.h"

#include <limits>

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
#include "ec2/synchronization_engine.h"
#include "email_manager.h"
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
    AbstractEmailManager* const emailManager,
    ec2::SyncronizationEngine* const ec2SyncronizationEngine) throw(std::runtime_error)
:
    m_settings(settings),
    m_timerManager(timerManager),
    m_accountManager(accountManager),
    m_systemHealthInfoProvider(systemHealthInfoProvider),
    m_dbManager(dbManager),
    m_emailManager(emailManager),
    m_ec2SyncronizationEngine(ec2SyncronizationEngine),
    m_dropSystemsTimerId(0),
    m_dropExpiredSystemsTaskStillRunning(false),
    m_systemDao(settings)
{
    using namespace std::placeholders;

    // Pre-filling cache.
    if (fillCache() != db::DBResult::ok)
        throw std::runtime_error("Failed to pre-load systems cache");

    m_dropSystemsTimerId =
        m_timerManager->addNonStopTimer(
            std::bind(&SystemManager::dropExpiredSystems, this, _1),
            m_settings.systemManager().dropExpiredSystemsPeriod,
            std::chrono::seconds::zero());

    // TODO: #ak All caches must be updated on successful commit, not on "transaction sent" handler.
    // Since it may lead to inconsistence between transactions and data cache (for a quite short period, though).

    // Registering transaction handler.
    m_ec2SyncronizationEngine->incomingTransactionDispatcher().registerTransactionHandler
        <::ec2::ApiCommand::saveUser, ::ec2::ApiUserData, data::SystemSharing>(
            std::bind(&SystemManager::processEc2SaveUser, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2SaveUserDone, this, _1, _2, _3));

    m_ec2SyncronizationEngine->incomingTransactionDispatcher().registerTransactionHandler
        <::ec2::ApiCommand::removeUser, ::ec2::ApiIdData, data::SystemSharing>(
            std::bind(&SystemManager::processEc2RemoveUser, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2RemoveUserDone, this, _1, _2, _3));

    // Currently this transaction can only rename some system.
    m_ec2SyncronizationEngine->incomingTransactionDispatcher().registerTransactionHandler
        <::ec2::ApiCommand::setResourceParam,
         ::ec2::ApiResourceParamWithRefData,
         data::SystemAttributesUpdate>(
            std::bind(&SystemManager::processSetResourceParam, this, _1, _2, _3, _4),
            std::bind(&SystemManager::onEc2SetResourceParamDone, this, _1, _2, _3));

    m_ec2SyncronizationEngine->incomingTransactionDispatcher().registerTransactionHandler
        <::ec2::ApiCommand::removeResourceParam,
         ::ec2::ApiResourceParamWithRefData,
         int>(
            std::bind(&SystemManager::processRemoveResourceParam, this, _1, _2, _3),
            std::bind(&SystemManager::onEc2RemoveResourceParamDone, this, _1, _2));

    m_accountManager->setUpdateAccountSubroutine(
        std::bind(&SystemManager::placeUpdateUserTransactionToEachSystem, this, _1, _2));
}

SystemManager::~SystemManager()
{
    m_timerManager->joinAndDeleteTimer(m_dropSystemsTimerId);

    m_startedAsyncCallsCounter.wait();

    m_accountManager->setUpdateAccountSubroutine(nullptr);
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
        [&completionHandler, &result]() { completionHandler(result); });

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
        cdb::attr::authSystemId,
        QString::fromStdString(systemIter->id));

    systemByIdIndex.modify(
        systemIter,
        [](data::SystemData& system){ system.status = api::SystemStatus::ssActivated; });

    using namespace std::placeholders;

    m_dbManager->executeUpdate<std::string>(
        std::bind(&dao::rdb::SystemDataObject::activateSystem, &m_systemDao, _1, _2),
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
    m_ec2SyncronizationEngine->transactionLog().startDbTransaction(
        newSystemDataPtr->systemData.id.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::unbindSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string>(
        std::bind(&SystemManager::markSystemAsDeleted, this, _1, _2),
        std::move(systemId.systemId),
        std::bind(&SystemManager::systemMarkedAsDeleted,
                    this, m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, std::move(completionHandler)));
}

namespace {
/**
 * Returns true, if record contains every single resource present in filter.
 */
bool applyFilter(
    const stree::AbstractResourceReader& record,
    const stree::AbstractIteratableContainer& filter)
{
    // TODO: #ak this method should be moved to stree and have linear complexity, not n*log(n)!
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
    auto systemId = wholeFilterMap.get<std::string>(cdb::attr::authSystemId);
    if (!systemId)
        systemId = wholeFilterMap.get<std::string>(cdb::attr::systemId);

    QnMutexLocker lk(&m_mutex);
    const auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    if (systemId)
    {
        //selecting system by id
        auto systemIter = systemByIdIndex.find(systemId.get());
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
            auto systemIter = systemByIdIndex.find(it->systemId);
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
            if (sharingData && sharingData->isEnabled)
            {
                systemDataEx.accessRole = sharingData->accessRole;
                // Calculating system weight.
                systemDataEx.usageFrequency = calculateSystemUsageFrequency(
                    sharingData->lastLoginTime,
                    sharingData->usageFrequency + 1);
                systemDataEx.lastLoginTime = sharingData->lastLoginTime;
                systemDataEx.sharingPermissions =
                    std::move(getSharingPermissions(systemDataEx.accessRole).accessRoles);
            }
            else
            {
                systemDataEx.accessRole = api::SystemAccessRole::none;
            }
        }

        // Omitting systems in which current user is disabled
        const auto unallowedSystemsRangeStartIter = std::remove_if(
            resultData.systems.begin(),
            resultData.systems.end(),
            [](const api::SystemDataEx& system)
            {
                return system.accessRole == api::SystemAccessRole::none;
            });
        resultData.systems.erase(unallowedSystemsRangeStartIter, resultData.systems.end());
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

    if (sharing.customPermissions.empty())
    {
        bool isAdmin = false;
        Qn::GlobalPermissions permissions(Qn::NoPermissions);
        ec2::accessRoleToPermissions(sharing.accessRole, &permissions, &isAdmin);
        sharing.customPermissions = QnLexical::serialized(permissions).toStdString();
    }
    sharing.vmsUserId = guidFromArbitraryData(
        sharing.accountEmail).toSimpleString().toStdString();

    auto dbUpdateFunc =
        [this, grantorEmail = std::move(grantorEmail), sharing = std::move(sharing)](
            nx::db::QueryContext* const queryContext) -> nx::db::DBResult
        {
            return updateSharingInDbAndGenerateTransaction(
                queryContext,
                grantorEmail,
                std::move(sharing),
                NotificationCommand::sendNotification);
        };

    auto onDbUpdateCompletedFunc =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* /*queryContext*/,
                nx::db::DBResult dbResult)
        {
            completionHandler(dbResultToApiResult(dbResult));
        };

    m_ec2SyncronizationEngine->transactionLog().startDbTransaction(
        sharing.systemId.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::getCloudUsersOfSystem(
    const AuthorizationInfo& authzInfo,
    const data::DataFilter& filter,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    api::SystemSharingExList resultData;

    auto authSystemId = authzInfo.get<std::string>(attr::authSystemId);

    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        if (!authSystemId)
            return completionHandler(api::ResultCode::notAuthorized, std::move(resultData));
    }

    auto systemId = authSystemId;
    if (!systemId)
        systemId = filter.get<std::string>(attr::systemId);

    QnMutexLocker lk(&m_mutex);

    if (systemId)
    {
        //selecting all sharings of system id
        const auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        const auto systemSharingsRange = systemIndex.equal_range(systemId.get());
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
            const auto systemSharingRange = systemIndex.equal_range(accountIter->systemId);
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
            sharingEx.accountId = account->id;
        }
    }

    completionHandler(
        api::ResultCode::ok,
        std::move(resultData));
}

void SystemManager::getAccessRoleList(
    const AuthorizationInfo& authzInfo,
    data::SystemId systemId,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
        return completionHandler(
            api::ResultCode::notAuthorized,
            api::SystemAccessRoleList());

    const auto accessRole = getAccountRightsForSystem(accountEmail, systemId.systemId);
    NX_LOGX(lm("account %1, system %2, account rights on system %3").
            arg(accountEmail).arg(systemId.systemId).arg(QnLexical::serialized(accessRole)),
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
constexpr const std::size_t kMaxOpaqueDataSize = 1024;

void SystemManager::updateSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemAttributesUpdate data,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_LOGX(lm("Updating system %1 sttributes").arg(data.systemId), cl_logDEBUG2);

    // Validating data received.
    if (data.name && (data.name->empty() || data.name->size() > kMaxSystemNameLength))
        return completionHandler(api::ResultCode::badRequest);
    if (data.opaque && (data.opaque->size() > kMaxOpaqueDataSize))
        return completionHandler(api::ResultCode::badRequest);

    const auto systemId = data.systemId;

    auto dbUpdateFunc =
        [this, data = std::move(data)](
            nx::db::QueryContext* const queryContext) mutable
    {
        return updateSystem(
            queryContext,
            std::move(data));
    };

    auto onDbUpdateCompletedFunc =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* /*queryContext*/,
                nx::db::DBResult dbResult)
        {
            completionHandler(dbResultToApiResult(dbResult));
        };

    m_ec2SyncronizationEngine->transactionLog().startDbTransaction(
        systemId.c_str(),
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
        userSessionDescriptor.systemId = authzInfo.get<std::string>(attr::authSystemId);
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
        std::bind(&SystemManager::saveUserSessionStart, this, _1, _2, _3),
        std::move(userSessionDescriptor),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* /*queryContext*/,
                nx::db::DBResult dbResult,
                data::UserSessionDescriptor /*userSessionDescriptor*/,
                SaveUserSessionResult /*result*/)
        {
            completionHandler(dbResultToApiResult(dbResult));
        });
}

api::SystemAccessRole SystemManager::getAccountRightsForSystem(
    const std::string& accountEmail,
    const std::string& systemId) const
{
#if 0
    QnMutexLocker lk(&m_mutex);
    const auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    api::SystemSharing toFind;
    toFind.accountEmail = accountEmail;
    toFind.systemId = systemId;
    const auto it = accountSystemPairIndex.find(toFind);
    return it != accountSystemPairIndex.end()
        ? it->accessRole
        : api::SystemAccessRole::none;
#else
    //TODO #ak getSystemSharingData does returns a lot of data which is redundant for this method.
    const auto sharingData = getSystemSharingData(accountEmail, systemId);
    if (!sharingData)
        return api::SystemAccessRole::none;
    return sharingData->accessRole;
#endif
}

boost::optional<api::SystemSharingEx> SystemManager::getSystemSharingData(
    const std::string& accountEmail,
    const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    const auto& accountSystemPairIndex =
        m_accountAccessRoleForSystem.get<kSharingUniqueIndex>();
    api::SystemSharing keyToFind;
    keyToFind.accountEmail = accountEmail;
    keyToFind.systemId = systemId;
    const auto it = accountSystemPairIndex.find(keyToFind);
    if (it == accountSystemPairIndex.end())
        return boost::none;

    api::SystemSharingEx resultData = *it;
    //resultData.api::SystemSharing::operator=(*it);

    const auto account = m_accountManager->findAccountByUserName(accountEmail);
    if (!account)
        return boost::none;

    resultData.accountId = account->id;
    resultData.accountFullName = account->fullName;
    return resultData;
}

nx::utils::Subscription<std::string>& SystemManager::systemMarkedAsDeletedSubscription()
{
    return m_systemMarkedAsDeletedSubscription;
}

const nx::utils::Subscription<std::string>&
    SystemManager::systemMarkedAsDeletedSubscription() const
{
    return m_systemMarkedAsDeletedSubscription;
}

std::pair<std::string, std::string> SystemManager::extractSystemIdAndVmsUserId(
    const api::SystemSharing& data)
{
    return std::make_pair(data.systemId, data.vmsUserId);
}

std::pair<std::string, std::string> SystemManager::extractSystemIdAndAccountEmail(
    const api::SystemSharing& data)
{
    return std::make_pair(data.systemId, data.accountEmail);
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

    dbResult = m_ec2SyncronizationEngine->transactionLog().updateTimestampHiForSystem(
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
    data::AccountData account;
    auto dbResult = m_accountManager->fetchAccountByEmail(
        queryContext, newSystem.accountEmail, &account);
    if (dbResult == db::DBResult::notFound)
    {
        NX_LOG(lm("Failed to add system %1 (%2) to account %3. Account not found")
            .arg(newSystem.name).arg(result->systemData.id).arg(newSystem.accountEmail),
            cl_logDEBUG1);
        return db::DBResult::notFound;
    }
    if (dbResult != db::DBResult::ok)
        return dbResult;

    NX_ASSERT(!result->systemData.id.empty());
    result->systemData.name = newSystem.name;
    result->systemData.customization = newSystem.customization;
    result->systemData.opaque = newSystem.opaque;
    result->systemData.authKey = QnUuid::createUuid().toSimpleString().toStdString();
    result->systemData.ownerAccountEmail = account.email;
    result->systemData.status = api::SystemStatus::ssNotActivated;
    result->systemData.expirationTimeUtc =
        nx::utils::timeSinceEpoch().count() +
        std::chrono::duration_cast<std::chrono::seconds>(
            m_settings.systemManager().notActivatedSystemLivePeriod).count();
    dbResult = m_systemDao.insert(queryContext, result->systemData, account.id);
    if (dbResult != db::DBResult::ok)
        return dbResult;

    return m_systemDao.selectSystemSequence(
        queryContext,
        result->systemData.id,
        &result->systemData.systemSequence);
}

nx::db::DBResult SystemManager::insertOwnerSharingToDb(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const data::SystemRegistrationDataWithAccount& newSystem,
    nx::cdb::data::SystemSharing* const ownerSharing)
{
    // Adding "owner" user to newly created system.
    ownerSharing->accountEmail = newSystem.accountEmail;
    ownerSharing->systemId = systemId;
    ownerSharing->accessRole = api::SystemAccessRole::owner;
    ownerSharing->vmsUserId = guidFromArbitraryData(
        ownerSharing->accountEmail).toSimpleString().toStdString();
    ownerSharing->isEnabled = true;
    ownerSharing->customPermissions = QnLexical::serialized(
        static_cast<Qn::GlobalPermissions>(Qn::GlobalAdminPermissionSet)).toStdString();
    const auto resultCode = updateSharingInDbAndGenerateTransaction(
        queryContext,
        newSystem.accountEmail,
        *ownerSharing,
        NotificationCommand::doNotSendNotification);
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
        m_accountAccessRoleForSystem.emplace(
            createDerivedFromBase(std::move(resultData.ownerSharing)));
    }
    completionHandler(
        dbResultToApiResult(dbResult),
        std::move(resultData.systemData));
}

nx::db::DBResult SystemManager::markSystemAsDeleted(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    auto dbResult = m_systemDao.markSystemAsDeleted(queryContext, systemId);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    dbResult = m_systemSharingDao.deleteSharing(queryContext, systemId);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

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
        markSystemAsDeletedInCache(systemId);

    m_systemMarkedAsDeletedSubscription.notify(systemId);

    completionHandler(dbResultToApiResult(dbResult));
}

void SystemManager::markSystemAsDeletedInCache(const std::string& systemId)
{
    using namespace std::chrono;

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
                    duration_cast<seconds>(
                        m_settings.systemManager().reportRemovedSystemPeriod).count();
            });
    }

    // Cleaning up system-to-account relation.
    auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
    systemIndex.erase(systemId);
}

nx::db::DBResult SystemManager::deleteSystemFromDB(
    nx::db::QueryContext* const queryContext,
    const data::SystemId& systemId)
{
    auto dbResult = m_systemSharingDao.deleteSharing(queryContext, systemId.systemId);
    if (dbResult != nx::db::DBResult::ok)
        return nx::db::DBResult::ioError;

    dbResult = m_systemDao.deleteSystem(queryContext, systemId.systemId);
    if (dbResult != nx::db::DBResult::ok)
        return nx::db::DBResult::ioError;

    return nx::db::DBResult::ok;
}

void SystemManager::systemDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemId systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        systemByIdIndex.erase(systemId.systemId);
        auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        systemIndex.erase(systemId.systemId);
    }
    completionHandler(dbResultToApiResult(dbResult));
}

nx::db::DBResult SystemManager::shareSystem(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    NotificationCommand notificationCommand,
    data::AccountData* const inviteeAccount)
{
    bool isNewAccount = false;
    nx::db::DBResult dbResult = fetchAccountToShareWith(
        queryContext, grantorEmail, sharing, inviteeAccount, &isNewAccount);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;
    if (isNewAccount)
        notificationCommand = NotificationCommand::doNotSendNotification;

    if (sharing.accessRole == api::SystemAccessRole::none)
    {
        return deleteSharing(
            queryContext,
            sharing.systemId,
            *inviteeAccount);
    }

    api::SystemSharingEx existingSharing;
    dbResult = m_systemSharingDao.fetchSharing(
        queryContext, inviteeAccount->email, sharing.systemId, &existingSharing);
    if (dbResult == nx::db::DBResult::ok)   //< Sharing already exists.
    {
        static_cast<api::SystemSharing&>(existingSharing) = sharing;
        return insertOrReplaceSharing(queryContext, std::move(existingSharing));
    }
    if (dbResult != nx::db::DBResult::notFound)
    {
        NX_LOGX(lm("Error fetching sharing (%1, %2) from Db")
            .arg(sharing.systemId).arg(inviteeAccount->email), cl_logDEBUG1);
        return dbResult;
    }

    api::SystemSharingEx sharingWithCalculatedData = createDerivedFromBase(sharing);
    sharingWithCalculatedData.lastLoginTime = std::chrono::system_clock::now();
    sharingWithCalculatedData.accountId = inviteeAccount->id;

    dbResult = m_systemSharingDao.calculateUsageFrequencyForANewSystem(
        queryContext,
        inviteeAccount->id,
        sharingWithCalculatedData.systemId,
        &sharingWithCalculatedData.usageFrequency);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error calculating usage frequency for sharing (%1, %2)")
            .arg(sharing.systemId).arg(inviteeAccount->email), cl_logDEBUG1);
        return dbResult;
    }

    dbResult = insertOrReplaceSharing(
        queryContext,
        std::move(sharingWithCalculatedData));
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error updating sharing (%1, %2) in Db")
            .arg(sharing.systemId).arg(inviteeAccount->email), cl_logDEBUG1);
        return dbResult;
    }

    if (notificationCommand == NotificationCommand::sendNotification)
    {
        dbResult = scheduleSystemHasBeenSharedNotification(
            queryContext, grantorEmail, sharing);
        if (dbResult != db::DBResult::ok)
            return dbResult;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::insertOrReplaceSharing(
    nx::db::QueryContext* const queryContext,
    api::SystemSharingEx sharing)
{
    auto dbResult = m_systemSharingDao.insertOrReplaceSharing(queryContext, sharing);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, sharing = std::move(sharing)]() mutable
        {
            updateSharingInCache(std::move(sharing));
        });

    return nx::db::DBResult::ok;
}

template<typename Notification>
nx::db::DBResult SystemManager::fillSystemSharedNotification(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const std::string& systemId,
    const std::string& inviteeEmail,
    Notification* const notification)
{
    // TODO: #ak: Fetching system name from DB to ensure data consistency.
    data::SystemData system;
    auto dbResult = fetchSystemById(queryContext, systemId, &system);
    if (dbResult != db::DBResult::ok)
        return dbResult;

    data::AccountData grantorAccount;
    dbResult = m_accountManager->fetchAccountByEmail(
        queryContext, grantorEmail, &grantorAccount);
    switch (dbResult)
    {
        case db::DBResult::ok:
            notification->message.sharer_name = grantorAccount.fullName;
            break;
        case db::DBResult::notFound:
            // It is ok if grantor is unknown (it can be local system user).
            break;
        default:
            return dbResult;
    }

    notification->customization = system.customization;
    notification->message.sharer_email = grantorEmail;
    notification->message.system_id = systemId;
    notification->message.system_name = system.name;
    notification->setAddressee(inviteeEmail);

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::scheduleSystemHasBeenSharedNotification(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const api::SystemSharing& sharing)
{
    auto notification = std::make_unique<SystemSharedNotification>();

    auto dbResult = fillSystemSharedNotification(
        queryContext,
        grantorEmail, sharing.systemId, sharing.accountEmail,
        notification.get());
    if (dbResult != db::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this,
            notification = std::move(notification),
            locker = m_startedAsyncCallsCounter.getScopedIncrement()]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });
    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchAccountToShareWith(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    data::AccountData* const inviteeAccount,
    bool* const isNewAccount)
{
    *isNewAccount = false;

    nx::db::DBResult dbResult = m_accountManager->fetchAccountByEmail(
        queryContext,
        sharing.accountEmail,
        inviteeAccount);
    switch (dbResult)
    {
        case nx::db::DBResult::ok:
            return nx::db::DBResult::ok;

        case nx::db::DBResult::notFound:
            if (sharing.accessRole == api::SystemAccessRole::none)
            {
                // Removing sharing, no sense to create account.
                return nx::db::DBResult::notFound;
            }
            break;

        default:
            NX_LOGX(lm("Error fetching account by email %1. %2")
                .arg(sharing.accountEmail).arg(QnLexical::serialized(dbResult)),
                cl_logDEBUG1);
            return dbResult;
    }

    // Initializing account customization for case if account has to be created.
    data::SystemData system;
    dbResult = fetchSystemById(queryContext, sharing.systemId, &system);
    if (dbResult != db::DBResult::ok)
        return dbResult;
    inviteeAccount->customization = system.customization;

    *isNewAccount = true;

    // Creating new account and sending invitation.
    inviteeAccount->id = m_accountManager->generateNewAccountId();
    inviteeAccount->email = sharing.accountEmail;
    inviteeAccount->statusCode = api::AccountStatus::invited;
    return inviteNewUserToSystem(
        queryContext,
        grantorEmail,
        *inviteeAccount,
        sharing.systemId);
}

nx::db::DBResult SystemManager::inviteNewUserToSystem(
    nx::db::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId)
{
    nx::db::DBResult dbResult =
        m_accountManager->insertAccount(queryContext, inviteeAccount);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    return scheduleInvintationNotificationDelivery(
        queryContext,
        inviterEmail,
        inviteeAccount,
        systemId);
}

nx::db::DBResult SystemManager::scheduleInvintationNotificationDelivery(
    nx::db::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId)
{
    auto notification = std::make_unique<InviteUserNotification>();
    auto dbResult = prepareInviteNotification(
        queryContext, inviterEmail, inviteeAccount, systemId, notification.get());
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this,
            notification = std::move(notification),
            locker = m_startedAsyncCallsCounter.getScopedIncrement()]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::prepareInviteNotification(
    nx::db::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId,
    InviteUserNotification* const notification)
{
    data::AccountConfirmationCode accountConfirmationCode;
    db::DBResult dbResult = m_accountManager->createPasswordResetCode(
        queryContext,
        inviteeAccount.email,
        &accountConfirmationCode);
    if (dbResult != db::DBResult::ok)
        return dbResult;

    dbResult = fillSystemSharedNotification(
        queryContext, inviterEmail, systemId, inviteeAccount.email, notification);
    if (dbResult != db::DBResult::ok)
        return dbResult;

    notification->setActivationCode(std::move(accountConfirmationCode.code));

    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::updateSharingInDbAndGenerateTransaction(
    nx::db::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    NotificationCommand notificationCommand)
{
    data::AccountData account;
    nx::db::DBResult result = shareSystem(
        queryContext,
        grantorEmail,
        sharing,
        notificationCommand,
        &account);
    if (result != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error sharing/unsharing system %1 with account %2. %3")
            .arg(sharing.systemId).arg(sharing.accountEmail)
            .arg(QnLexical::serialized(result)),
            cl_logDEBUG1);
        return result;
    }

    if (sharing.accessRole != api::SystemAccessRole::none)
    {
        result = generateSaveUserTransaction(queryContext, sharing, account);
        if (result != nx::db::DBResult::ok)
            return result;

        result = generateUpdateFullNameTransaction(queryContext, sharing, account.fullName);
        if (result != nx::db::DBResult::ok)
            return result;
    }
    else
    {
        result = generateRemoveUserTransaction(queryContext, sharing);
        if (result != nx::db::DBResult::ok)
            return result;

        result = generateRemoveUserFullNameTransaction(queryContext, sharing);
        if (result != nx::db::DBResult::ok)
            return result;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemManager::generateSaveUserTransaction(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    const api::AccountData& account)
{
    ::ec2::ApiUserData userData;
    ec2::convert(sharing, &userData);
    userData.isCloud = true;
    userData.fullName = QString::fromStdString(account.fullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::saveUser>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(userData));
}

nx::db::DBResult SystemManager::generateUpdateFullNameTransaction(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    const std::string& newFullName)
{
    //generating "save full name" transaction
    ::ec2::ApiResourceParamWithRefData fullNameData;
    fullNameData.resourceId = QnUuid(sharing.vmsUserId.c_str());
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(newFullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(fullNameData));
}

nx::db::DBResult SystemManager::generateRemoveUserTransaction(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharing& sharing)
{
    ::ec2::ApiIdData userId;
    ec2::convert(sharing, &userId);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::removeUser>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(userId));
}

nx::db::DBResult SystemManager::generateRemoveUserFullNameTransaction(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharing& sharing)
{
    ::ec2::ApiResourceParamWithRefData fullNameParam;
    fullNameParam.resourceId = QnUuid(sharing.vmsUserId.c_str());
    fullNameParam.name = Qn::USER_FULL_NAME;
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::removeResourceParam>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(fullNameParam));
}

nx::db::DBResult SystemManager::placeUpdateUserTransactionToEachSystem(
    nx::db::QueryContext* const queryContext,
    const data::AccountUpdateDataWithEmail& accountUpdate)
{
    if (!accountUpdate.fullName)
        return nx::db::DBResult::ok;

    std::deque<api::SystemSharingEx> sharings;
    auto dbResult = m_systemSharingDao.fetchUserSharingsByAccountEmail(
        queryContext, accountUpdate.email, &sharings);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;
    for (const auto& sharing: sharings)
    {
        dbResult = generateUpdateFullNameTransaction(
            queryContext, sharing, *accountUpdate.fullName);
        if (dbResult != nx::db::DBResult::ok)
            return dbResult;
    }

    return nx::db::DBResult::ok;
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

nx::db::DBResult SystemManager::updateSystem(
    nx::db::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    nx::db::DBResult dbResult = nx::db::DBResult::ok;
    if (data.name)
    {
        dbResult = renameSystem(queryContext, data);
        if (dbResult != nx::db::DBResult::ok)
            return dbResult;
    }

    if (data.opaque)
    {
        dbResult = m_systemDao.execSystemOpaqueUpdate(queryContext, data);
        if (dbResult != nx::db::DBResult::ok)
            return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, data]() mutable
        {
            updateSystemAttributesInCache(std::move(data));
        });

    return dbResult;
}

nx::db::DBResult SystemManager::renameSystem(
    nx::db::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    NX_LOGX(lm("Renaming system %1 to %2")
        .arg(data.systemId).arg(data.name.get()), cl_logDEBUG2);

    const auto result = m_systemDao.execSystemNameUpdate(queryContext, data);
    if (result != db::DBResult::ok)
        return result;

    // Generating transaction.
    ::ec2::ApiResourceParamWithRefData systemNameData;
    systemNameData.resourceId = QnUserResource::kAdminGuid;
    systemNameData.name = nx::settings_names::kNameSystemName;
    systemNameData.value = QString::fromStdString(data.name.get());

    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
        ::ec2::ApiCommand::setResourceParam>(
            queryContext,
            data.systemId.c_str(),
            std::move(systemNameData));
}

void SystemManager::updateSystemAttributesInCache(
    data::SystemAttributesUpdate data)
{
    //updating system name in cache
    QnMutexLocker lk(&m_mutex);

    auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    auto systemIter = systemByIdIndex.find(data.systemId);
    if (systemIter != systemByIdIndex.end())
    {
        systemByIdIndex.modify(
            systemIter,
            [&data](data::SystemData& system)
            {
                if (data.name)
                    system.name = std::move(data.name.get());
                if (data.opaque)
                    system.opaque = std::move(data.opaque.get());
            });
    }
}

void SystemManager::systemNameUpdated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemAttributesUpdate data,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSystemAttributesInCache(std::move(data));
    }

    completionHandler(
        dbResult == nx::db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
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

    completionHandler(dbResultToApiResult(dbResult));
}

constexpr const int kSecondsPerDay = 60*60*24;

nx::db::DBResult SystemManager::saveUserSessionStart(
    nx::db::QueryContext* queryContext,
    const data::UserSessionDescriptor& userSessionDescriptor,
    SaveUserSessionResult* const usageStatistics)
{
    using namespace std::chrono;

    NX_ASSERT(userSessionDescriptor.accountEmail && userSessionDescriptor.systemId);

    api::SystemSharingEx sharing;
    auto dbResult = m_systemSharingDao.fetchSharing(
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
    usageStatistics->lastloginTime = nx::utils::utcTime();
    usageStatistics->usageFrequency = calculateSystemUsageFrequency(
        sharing.lastLoginTime,
        sharing.usageFrequency);
    const auto newUsageFrequency = usageStatistics->usageFrequency + 1;

    // Saving new statistics to DB.
    dbResult = m_systemSharingDao.updateUserLoginStatistics(
        queryContext,
        sharing.accountId,
        sharing.systemId,
        usageStatistics->lastloginTime,
        newUsageFrequency);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Error updating user login statistics"), cl_logWARNING);
        return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, userSessionDescriptor, usageStatistics = *usageStatistics]()
        {
            updateSystemUsageStatisticsCache(
                userSessionDescriptor,
                usageStatistics);
        });

    return nx::db::DBResult::ok;
}

void SystemManager::updateSystemUsageStatisticsCache(
    const data::UserSessionDescriptor& userSessionDescriptor,
    const SaveUserSessionResult& usageStatistics)
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
            [&usageStatistics](api::SystemSharingEx& systemSharing)
            {
                systemSharing.usageFrequency = usageStatistics.usageFrequency;
                systemSharing.lastLoginTime = usageStatistics.lastloginTime;
            });
    }
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
    using namespace std::placeholders;

    std::vector<data::SystemData> systems;
    auto result = doBlockingDbQuery(
        std::bind(&dao::rdb::SystemDataObject::fetchSystems, m_systemDao,
            _1, db::InnerJoinFilterFields(), &systems));
    if (result != db::DBResult::ok)
        return result;

    for (auto& system: systems)
    {
        auto idCopy = system.id;
        if (!m_systems.insert(std::move(system)).second)
        {
            NX_ASSERT(false);
        }
    }

    result = doBlockingDbQuery(
        std::bind(&SystemManager::fetchSystemToAccountBinder, this, _1));
    if (result != db::DBResult::ok)
        return result;

    return db::DBResult::ok;
}

template<typename Func>
nx::db::DBResult SystemManager::doBlockingDbQuery(Func func)
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    m_dbManager->executeSelect(
        std::move(func),
        [&cacheFilledPromise](
            nx::db::QueryContext* /*queryContext*/,
            db::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });
    //waiting for completion
    return future.get();
}

nx::db::DBResult SystemManager::fetchSystemById(
    nx::db::QueryContext* queryContext,
    const std::string& systemId,
    data::SystemData* const system)
{
    const nx::db::InnerJoinFilterFields sqlFilter =
        {{"system.id", ":systemId", QnSql::serialized_field(systemId)}};

    std::vector<data::SystemData> systems;
    auto dbResult = m_systemDao.fetchSystems(queryContext, sqlFilter, &systems);
    if (dbResult != db::DBResult::ok)
        return dbResult;
    if (systems.empty())
        return db::DBResult::notFound;
    NX_ASSERT(systems.size() == 1);
    *system = std::move(systems[0]);
    return db::DBResult::ok;
}

nx::db::DBResult SystemManager::fetchSystemToAccountBinder(
    nx::db::QueryContext* queryContext)
{
    // TODO: #ak Do it without 

    std::deque<api::SystemSharingEx> sharings;
    const auto result = m_systemSharingDao.fetchAllUserSharings(
        queryContext, &sharings);
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
        std::bind(&dao::rdb::SystemDataObject::deleteExpiredSystems, m_systemDao, _1),
        std::bind(&SystemManager::expiredSystemsDeletedFromDb, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2));
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
    ::ec2::QnTransaction<::ec2::ApiUserData> transaction,
    data::SystemSharing* const systemSharingData)
{
    const auto& vmsUser = transaction.params;

    NX_LOGX(lm("Processing vms transaction saveUser. "
        "user %1, systemId %2, permissions %3, enabled %4")
        .arg(vmsUser.email).arg(systemId)
        .arg(QnLexical::serialized(vmsUser.permissions)).arg(vmsUser.isEnabled),
        cl_logDEBUG2);

    // Preparing SystemSharing structure.
    systemSharingData->systemId = systemId.toStdString();
    ec2::convert(vmsUser, systemSharingData);

    // We have no information about grantor here. Using system owner...
    const nx::db::InnerJoinFilterFields sqlFilter =
        {{"vms_user_id", ":vmsUserId",
           QnSql::serialized_field(transaction.historyAttributes.author.toSimpleString())}};
    api::SystemSharingEx grantorInfo;
    auto dbResult = m_systemSharingDao.fetchSharing(
        queryContext,
        sqlFilter,
        &grantorInfo);
    if (dbResult != nx::db::DBResult::ok &&
        dbResult != nx::db::DBResult::notFound)
    {
        NX_LOGX(lm("Error fetching saveUser transaction author. systemId %1")
            .arg(systemId), cl_logDEBUG1);
        return dbResult;
    }
    //< System could have been shared by a local user, so we will not find him in our DB.

    // Updating db.
    data::AccountData account;
    dbResult = shareSystem(
        queryContext,
        grantorInfo.accountEmail,
        *systemSharingData,
        NotificationCommand::sendNotification,
        &account);
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    // Generating "save full name" transaction.
    ::ec2::ApiResourceParamWithRefData fullNameData;
    fullNameData.resourceId = vmsUser.id;
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(account.fullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog<
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
    ::ec2::QnTransaction<::ec2::ApiIdData> transaction,
    data::SystemSharing* const systemSharingData)
{
    const auto& data = transaction.params;

    NX_LOGX(
        QnLog::EC2_TRAN_LOG,
        lm("Processing vms transaction removeUser. systemId %1, vms user id %2")
            .arg(systemId).str(data.id),
        cl_logDEBUG2);

    systemSharingData->systemId = systemId.toStdString();
    systemSharingData->vmsUserId = data.id.toSimpleString().toStdString();

    const auto dbResult = m_systemSharingDao.deleteSharing(
        queryContext,
        systemId.toStdString(),
        {{"vms_user_id", ":vmsUserId",
          QnSql::serialized_field(systemSharingData->vmsUserId)}});
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

nx::db::DBResult SystemManager::deleteSharing(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const data::AccountData& inviteeAccount)
{
    const auto dbResult = m_systemSharingDao.deleteSharing(
        queryContext,
        systemId,
        {{"account_id", ":accountId", QnSql::serialized_field(inviteeAccount.id)}});
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    api::SystemSharingEx sharing;
    sharing.accountEmail = inviteeAccount.email;
    sharing.accountId = inviteeAccount.id;
    sharing.systemId = systemId;
    sharing.accessRole = api::SystemAccessRole::none;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, sharing = std::move(sharing)]() mutable
        {
            updateSharingInCache(std::move(sharing));
        });

    return dbResult;
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
        std::make_pair(sharing.systemId, sharing.vmsUserId));
}

nx::db::DBResult SystemManager::processSetResourceParam(
    nx::db::QueryContext* queryContext,
    const nx::String& systemId,
    ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> transaction,
    data::SystemAttributesUpdate* const systemNameUpdate)
{
    const auto& data = transaction.params;

    if (data.resourceId != QnUserResource::kAdminGuid ||
        data.name != nx::settings_names::kNameSystemName)
    {
        NX_LOGX(
            QnLog::EC2_TRAN_LOG,
            lm("Ignoring transaction setResourceParam with "
               "systemId %1, resourceId %2, param name %3, param value %4")
                .arg(systemId).arg(data.resourceId).arg(data.name).arg(data.value),
            cl_logDEBUG2);
        return nx::db::DBResult::ok;
    }

    systemNameUpdate->systemId = systemId.toStdString();
    systemNameUpdate->name = data.value.toStdString();
    return m_systemDao.execSystemNameUpdate(queryContext, *systemNameUpdate);
}

void SystemManager::onEc2SetResourceParamDone(
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult dbResult,
    data::SystemAttributesUpdate systemNameUpdate)
{
    if (dbResult == nx::db::DBResult::ok)
    {
        updateSystemAttributesInCache(std::move(systemNameUpdate));
    }
}

nx::db::DBResult SystemManager::processRemoveResourceParam(
    nx::db::QueryContext* /*queryContext*/,
    const nx::String& systemId,
    ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> data)
{
    // This can only be removal of already-removed user attribute.
    NX_LOGX(
        QnLog::EC2_TRAN_LOG,
        lm("Ignoring transaction %1 with "
            "systemId %2, resourceId %3, param name %4")
            .arg(::ec2::ApiCommand::toString(data.command)).arg(systemId)
            .arg(data.params.resourceId).arg(data.params.name),
        cl_logDEBUG2);
    return nx::db::DBResult::ok;
}

void SystemManager::onEc2RemoveResourceParamDone(
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult /*dbResult*/)
{
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

} // namespace cdb
} // namespace nx
