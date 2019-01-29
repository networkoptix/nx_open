#include "system_manager.h"

#include <limits>

#include <nx/vms/api/data/resource_data.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/system_utils.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/db/ec2/data_conversion.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include <api/global_settings.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>
#include <utils/common/id.h>

#include "account_manager.h"
#include "email_manager.h"
#include "system_health_info_provider.h"
#include "../access_control/authentication_manager.h"
#include "../access_control/authorization_manager.h"
#include "../ec2/vms_command_descriptor.h"
#include "../settings.h"
#include "../stree/cdb_ns.h"

namespace nx::cloud::db {

// TODO: #ak should get rid of following function
static api::SystemSharingEx createDerivedFromBase(api::SystemSharing right)
{
    api::SystemSharingEx result;
    result.api::SystemSharing::operator=(std::move(right));
    return result;
}

SystemManager::SystemManager(
    const conf::Settings& settings,
    nx::utils::StandaloneTimerManager* const timerManager,
    AbstractAccountManager* const accountManager,
    const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
    nx::sql::AsyncSqlQueryExecutor* const dbManager,
    AbstractEmailManager* const emailManager,
    clusterdb::engine::SyncronizationEngine* const ec2SyncronizationEngine) noexcept(false)
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
    m_systemDao(dao::SystemDataObjectFactory::create(settings))
{
    using namespace std::placeholders;

    // Pre-filling cache.
    if (fillCache() != nx::sql::DBResult::ok)
        throw std::runtime_error("Failed to pre-load systems cache");

    m_dropSystemsTimerId =
        m_timerManager->addNonStopTimer(
            std::bind(&SystemManager::dropExpiredSystems, this, _1),
            m_settings.systemManager().dropExpiredSystemsPeriod,
            std::chrono::seconds::zero());

    // TODO: #ak All caches must be updated on successful commit, not on "transaction sent" handler.
    // Since it may lead to inconsistence between transactions and data cache (for a quite short period, though).

    // Registering transaction handler.
    m_ec2SyncronizationEngine->incomingCommandDispatcher().registerCommandHandler
        <ec2::command::SaveUser>(
            std::bind(&SystemManager::processEc2SaveUser, this, _1, _2, _3));

    m_ec2SyncronizationEngine->incomingCommandDispatcher().registerCommandHandler
        <ec2::command::RemoveUser>(
            std::bind(&SystemManager::processEc2RemoveUser, this, _1, _2, _3));

    // Currently this transaction can only rename some system.
    m_ec2SyncronizationEngine->incomingCommandDispatcher().registerCommandHandler
        <ec2::command::SetResourceParam>(
            std::bind(&SystemManager::processSetResourceParam, this, _1, _2, _3));

    m_ec2SyncronizationEngine->incomingCommandDispatcher().registerCommandHandler
        <ec2::command::RemoveResourceParam>(
            std::bind(&SystemManager::processRemoveResourceParam, this, _1, _2, _3));

    m_accountManager->addExtension(this);
}

SystemManager::~SystemManager()
{
    m_ec2SyncronizationEngine->incomingCommandDispatcher().removeHandler
        <ec2::command::RemoveResourceParam>();

    m_ec2SyncronizationEngine->incomingCommandDispatcher().removeHandler
        <ec2::command::SetResourceParam>();

    m_ec2SyncronizationEngine->incomingCommandDispatcher().removeHandler
        <ec2::command::RemoveUser>();

    m_ec2SyncronizationEngine->incomingCommandDispatcher().removeHandler
        <ec2::command::SaveUser>();

    m_timerManager->joinAndDeleteTimer(m_dropSystemsTimerId);

    m_startedAsyncCallsCounter.wait();

    m_accountManager->removeExtension(this);
}

void SystemManager::authenticateByName(
    const nx::network::http::StringType& username,
    const std::function<bool(const nx::Buffer&)>& validateHa1Func,
    nx::utils::stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::Result)> completionHandler)
{
    api::ResultCode result = api::ResultCode::notAuthorized;
    auto scopedGuard = nx::utils::makeScopeGuard(
        [&completionHandler, &result]() { completionHandler(result); });

    QnMutexLocker lock(&m_mutex);

    auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    const auto systemIter = systemByIdIndex.find(username.toStdString());
    if (systemIter == systemByIdIndex.end())
    {
        result = api::ResultCode::badUsername;
        return;
    }

    if (systemIter->status == api::SystemStatus::deleted_)
    {
        if (systemIter->expirationTimeUtc > nx::utils::timeSinceEpoch().count() ||
            m_settings.systemManager().controlSystemStatusByDb) //system not expired yet
        {
            result = api::ResultCode::credentialsRemovedPermanently;
        }
        return;
    }

    if (!validateHa1Func(
            nx::network::http::calcHa1(
                username,
                AuthenticationManager::realm(),
                nx::String(systemIter->authKey.c_str()))))
    {
        return;
    }

    authProperties->put(
        nx::cloud::db::attr::authSystemId,
        QString::fromStdString(systemIter->id));

    result = api::ResultCode::ok;

    activateSystemIfNeeded(lock, systemByIdIndex, systemIter);
}

void SystemManager::bindSystemToAccount(
    const AuthorizationInfo& authzInfo,
    data::SystemRegistrationData registrationData,
    std::function<void(api::Result, data::SystemData)> completionHandler)
{
    QString accountEmail;
    if (!authzInfo.get(nx::cloud::db::attr::authAccountEmail, &accountEmail))
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
            nx::sql::QueryContext* const queryContext) -> nx::sql::DBResult
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
                nx::sql::DBResult dbResult)
        {
            systemAdded(
                std::move(locker),
                dbResult,
                std::move(*registrationDataWithAccount),
                std::move(*newSystemData),
                std::move(completionHandler));
        };

    // Creating db transaction via CommandLog.
    m_ec2SyncronizationEngine->transactionLog().startDbTransaction(
        newSystemDataPtr->systemData.id.c_str(),
        std::move(dbUpdateFunc),
        std::move(onDbUpdateCompletedFunc));
}

void SystemManager::unbindSystem(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId systemId,
    std::function<void(api::Result)> completionHandler)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string>(
        std::bind(&SystemManager::markSystemForDeletion, this, _1, _2),
        std::move(systemId.systemId),
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult,
                std::string /*systemId*/)
        {
            completionHandler(dbResultToApiResult(dbResult));
        });
}

void SystemManager::getSystems(
    const AuthorizationInfo& authzInfo,
    data::DataFilter filter,
    std::function<void(api::Result, api::SystemDataExList)> completionHandler)
{
    // Always providing only activated systems.
    filter.addFilterValue(
        attr::systemStatus,
        static_cast<int>(api::SystemStatus::activated));
    filter.addFilterValue(
        attr::systemStatus,
        static_cast<int>(api::SystemStatus::beingMerged));

    nx::utils::stree::MultiSourceResourceReader wholeFilterMap(filter, authzInfo);

    const auto foundSystems = selectSystemsFromCacheByFilter(wholeFilterMap, filter);
    if (!foundSystems)
        return completionHandler(api::ResultCode::notFound, api::SystemDataExList());

    api::SystemDataExList resultData;
    resultData.systems = std::move(*foundSystems);

    const auto accountEmail = wholeFilterMap.get<std::string>(nx::cloud::db::attr::authAccountEmail);
    if (accountEmail)
    {
        std::for_each(
            resultData.systems.begin(),
            resultData.systems.end(),
            std::bind(&SystemManager::addUserAccessInfo, this,
                *accountEmail, std::placeholders::_1));

        // Omitting systems in which current user is disabled.
        const auto unallowedSystemsRangeStartIter = std::remove_if(
            resultData.systems.begin(),
            resultData.systems.end(),
            [](const api::SystemDataEx& system)
            {
                return system.accessRole == api::SystemAccessRole::none;
            });
        resultData.systems.erase(unallowedSystemsRangeStartIter, resultData.systems.end());

        // If every system has been filtered out by access rights, then reporting forbidden.
        if (resultData.systems.empty() &&
            static_cast<bool>(filter.get<std::string>(attr::systemId)))
        {
            return completionHandler(api::ResultCode::forbidden, resultData);
        }
    }

    // Adding system health.
    for (auto& systemDataEx: resultData.systems)
    {
        systemDataEx.stateOfHealth =
            m_systemHealthInfoProvider.isSystemOnline(systemDataEx.id)
            ? api::SystemHealth::online
            : api::SystemHealth::offline;

        m_extensions.invoke(
            &AbstractSystemExtension::modifySystemBeforeProviding, &systemDataEx);
    }

    completionHandler(api::ResultCode::ok, resultData);
}

void SystemManager::shareSystem(
    const AuthorizationInfo& authzInfo,
    data::SystemSharing sharing,
    std::function<void(api::Result)> completionHandler)
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
        GlobalPermissions permissions(Qn::NoPermissions);
        ec2::accessRoleToPermissions(sharing.accessRole, &permissions, &isAdmin);
        sharing.customPermissions = QnLexical::serialized(permissions).toStdString();
    }
    sharing.vmsUserId = guidFromArbitraryData(
        sharing.accountEmail).toSimpleString().toStdString();

    auto dbUpdateFunc =
        [this, grantorEmail = std::move(grantorEmail), sharing = std::move(sharing)](
            nx::sql::QueryContext* const queryContext) -> nx::sql::DBResult
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
                nx::sql::DBResult dbResult)
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
    std::function<void(api::Result, api::SystemSharingExList)> completionHandler)
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
    std::function<void(api::Result, api::SystemAccessRoleList)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
        return completionHandler(
            api::ResultCode::notAuthorized,
            api::SystemAccessRoleList());

    const auto accessRole = getAccountRightsForSystem(accountEmail, systemId.systemId);
    NX_VERBOSE(this, lm("account %1, system %2, account rights on system %3").
            arg(accountEmail).arg(systemId.systemId).arg(QnLexical::serialized(accessRole)));
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
    std::function<void(api::Result)> completionHandler)
{
    NX_VERBOSE(this, lm("Updating system %1 attributes").arg(data.systemId));

    // Validating data received.
    if (data.name && (data.name->empty() || data.name->size() > kMaxSystemNameLength))
        return completionHandler(api::ResultCode::badRequest);
    if (data.opaque && (data.opaque->size() > kMaxOpaqueDataSize))
        return completionHandler(api::ResultCode::badRequest);

    const auto systemId = data.systemId;

    auto dbUpdateFunc =
        [this, data = std::move(data)](
            nx::sql::QueryContext* const queryContext) mutable
    {
        return updateSystem(
            queryContext,
            std::move(data));
    };

    auto onDbUpdateCompletedFunc =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult)
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
    std::function<void(api::Result)> completionHandler)
{
    NX_ASSERT(userSessionDescriptor.systemId || userSessionDescriptor.accountEmail);
    if (!(userSessionDescriptor.systemId || userSessionDescriptor.accountEmail))
        return completionHandler(api::ResultCode::unknownError);

    if (!userSessionDescriptor.systemId)
    {
        userSessionDescriptor.systemId = authzInfo.get<std::string>(attr::authSystemId);
        if (!userSessionDescriptor.systemId)
        {
            NX_DEBUG(this, lm("system id may be absent in request parameters "
                "only if it has been used as a login"));
            return completionHandler(api::ResultCode::forbidden);
        }
    }

    if (!userSessionDescriptor.accountEmail)
    {
        userSessionDescriptor.accountEmail =
            authzInfo.get<std::string>(attr::authAccountEmail);
        if (!userSessionDescriptor.accountEmail)
        {
            NX_DEBUG(this, lm("account email may be absent in request parameters "
                "only if it has been used as a login"));
            return completionHandler(api::ResultCode::forbidden);
        }
    }

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::UserSessionDescriptor, SaveUserSessionResult>(
        std::bind(&SystemManager::saveUserSessionStart, this, _1, _2, _3),
        std::move(userSessionDescriptor),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult,
                data::UserSessionDescriptor /*userSessionDescriptor*/,
                SaveUserSessionResult /*result*/)
        {
            completionHandler(dbResultToApiResult(dbResult));
        });
}

boost::optional<api::SystemData> SystemManager::findSystemById(const std::string& id) const
{
    QnMutexLocker lock(&m_mutex);

    const auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    const auto systemIter = systemByIdIndex.find(id);
    if (systemIter == systemByIdIndex.end())
        return boost::none;
    return *systemIter;
}

nx::sql::DBResult SystemManager::fetchSystemById(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    data::SystemData* const system)
{
    const nx::sql::InnerJoinFilterFields sqlFilter =
        {nx::sql::SqlFilterFieldEqual(
            "system.id", ":systemId", QnSql::serialized_field(systemId))};

    std::vector<data::SystemData> systems;
    auto dbResult = m_systemDao->fetchSystems(queryContext, sqlFilter, &systems);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;
    if (systems.empty())
        return nx::sql::DBResult::notFound;
    NX_ASSERT(systems.size() == 1);
    *system = std::move(systems[0]);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::updateSystemStatus(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    api::SystemStatus systemStatus)
{
    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&SystemManager::updateSystemStatusInCache, this,
            systemId, systemStatus));

    return m_systemDao->updateSystemStatus(queryContext, systemId, systemStatus);
}

nx::sql::DBResult SystemManager::markSystemForDeletion(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    auto dbResult = m_systemDao->markSystemForDeletion(queryContext, systemId);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = m_systemSharingDao.deleteSharing(queryContext, systemId);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, systemId]()
        {
            markSystemForDeletionInCache(systemId);
            m_systemMarkedAsDeletedSubscription.notify(systemId);
        });

    return nx::sql::DBResult::ok;
}

api::SystemAccessRole SystemManager::getAccountRightsForSystem(
    const std::string& accountEmail,
    const std::string& systemId) const
{
    const auto sharingData = getSystemSharingData(accountEmail, systemId);
    if (!sharingData)
        return api::SystemAccessRole::none;
    return sharingData->accessRole;
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

std::vector<api::SystemSharingEx> SystemManager::fetchSystemUsers(
    sql::QueryContext* queryContext,
    const std::string& systemId)
{
    return m_systemSharingDao.fetchSystemSharings(queryContext, systemId);
}

std::vector<api::SystemSharingEx> SystemManager::fetchAllSharings() const
{
    QnMutexLocker lock(&m_mutex);

    std::vector<api::SystemSharingEx> sharings;
    sharings.reserve(m_accountAccessRoleForSystem.size());
    std::copy(
        m_accountAccessRoleForSystem.begin(),
        m_accountAccessRoleForSystem.end(),
        std::back_inserter(sharings));

    return sharings;
}

nx::utils::Subscription<std::string>& SystemManager::systemMarkedAsDeletedSubscription()
{
    return m_systemMarkedAsDeletedSubscription;
}

void SystemManager::addSystemSharingExtension(AbstractSystemSharingExtension* extension)
{
    m_systemSharingExtensions.insert(extension);
}

void SystemManager::removeSystemSharingExtension(AbstractSystemSharingExtension* extension)
{
    m_systemSharingExtensions.erase(extension);
}

void SystemManager::addExtension(AbstractSystemExtension* extension)
{
    m_extensions.add(extension);
}

void SystemManager::removeExtension(AbstractSystemExtension* extension)
{
    m_extensions.remove(extension);
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

void SystemManager::afterUpdatingAccount(
    nx::sql::QueryContext* queryContext,
    const data::AccountUpdateDataWithEmail& accountUpdate)
{
    placeUpdateUserTransactionToEachSystem(queryContext, accountUpdate);
}

nx::sql::DBResult SystemManager::insertSystemToDB(
    nx::sql::QueryContext* const queryContext,
    const data::SystemRegistrationDataWithAccount& newSystem,
    InsertNewSystemToDbResult* const result)
{
    nx::sql::DBResult dbResult = nx::sql::DBResult::ok;

    dbResult = insertNewSystemDataToDb(queryContext, newSystem, result);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = m_ec2SyncronizationEngine->transactionLog().updateTimestampHiForSystem(
        queryContext,
        result->systemData.id.c_str(),
        result->systemData.systemSequence);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = insertOwnerSharingToDb(
        queryContext, result->systemData.id, newSystem, &result->ownerSharing);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::insertNewSystemDataToDb(
    nx::sql::QueryContext* const queryContext,
    const data::SystemRegistrationDataWithAccount& newSystem,
    InsertNewSystemToDbResult* const result)
{
    data::AccountData account;
    auto dbResult = m_accountManager->fetchAccountByEmail(
        queryContext, newSystem.accountEmail, &account);
    if (dbResult == nx::sql::DBResult::notFound)
    {
        NX_DEBUG(this, lm("Failed to add system %1 (%2) to account %3. Account not found")
            .arg(newSystem.name).arg(result->systemData.id).arg(newSystem.accountEmail));
        return nx::sql::DBResult::notFound;
    }
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    NX_ASSERT(!result->systemData.id.empty());
    result->systemData.name = newSystem.name;
    result->systemData.customization = newSystem.customization;
    result->systemData.opaque = newSystem.opaque;
    result->systemData.authKey = QnUuid::createUuid().toSimpleString().toStdString();
    result->systemData.ownerAccountEmail = account.email;
    result->systemData.status = api::SystemStatus::notActivated;
    result->systemData.expirationTimeUtc =
        nx::utils::timeSinceEpoch().count() +
        std::chrono::duration_cast<std::chrono::seconds>(
            m_settings.systemManager().notActivatedSystemLivePeriod).count();
    result->systemData.registrationTime = nx::utils::utcTime();
    dbResult = m_systemDao->insert(queryContext, result->systemData, account.id);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    return m_systemDao->selectSystemSequence(
        queryContext,
        result->systemData.id,
        &result->systemData.systemSequence);
}

nx::sql::DBResult SystemManager::insertOwnerSharingToDb(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const data::SystemRegistrationDataWithAccount& newSystem,
    nx::cloud::db::data::SystemSharing* const ownerSharing)
{
    // Adding "owner" user to newly created system.
    ownerSharing->accountEmail = newSystem.accountEmail;
    ownerSharing->systemId = systemId;
    ownerSharing->accessRole = api::SystemAccessRole::owner;
    ownerSharing->vmsUserId = guidFromArbitraryData(
        ownerSharing->accountEmail).toSimpleString().toStdString();
    ownerSharing->isEnabled = true;
    ownerSharing->customPermissions = QnLexical::serialized(
        static_cast<GlobalPermissions>(GlobalPermission::adminPermissions)).toStdString();
    const auto resultCode = updateSharingInDbAndGenerateTransaction(
        queryContext,
        newSystem.accountEmail,
        *ownerSharing,
        NotificationCommand::doNotSendNotification);
    if (resultCode != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Could not insert system %1 to account %2 binding into DB. %3").
            arg(newSystem.name).arg(newSystem.accountEmail).
            arg(queryContext->connection()->lastErrorText()));
        return resultCode;
    }

    return nx::sql::DBResult::ok;
}

void SystemManager::systemAdded(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::sql::DBResult dbResult,
    data::SystemRegistrationDataWithAccount /*systemRegistrationData*/,
    InsertNewSystemToDbResult resultData,
    std::function<void(api::Result, data::SystemData)> completionHandler)
{
    if (dbResult == nx::sql::DBResult::ok)
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

void SystemManager::markSystemForDeletionInCache(const std::string& systemId)
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
                system.status = api::SystemStatus::deleted_;
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

nx::sql::DBResult SystemManager::deleteSystemFromDB(
    nx::sql::QueryContext* const queryContext,
    const data::SystemId& systemId)
{
    auto dbResult = m_systemSharingDao.deleteSharing(queryContext, systemId.systemId);
    if (dbResult != nx::sql::DBResult::ok)
        return nx::sql::DBResult::ioError;

    dbResult = m_systemDao->deleteSystem(queryContext, systemId.systemId);
    if (dbResult != nx::sql::DBResult::ok)
        return nx::sql::DBResult::ioError;

    return nx::sql::DBResult::ok;
}

void SystemManager::systemDeleted(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::sql::DBResult dbResult,
    data::SystemId systemId,
    std::function<void(api::Result)> completionHandler)
{
    if (dbResult == nx::sql::DBResult::ok)
    {
        QnMutexLocker lk(&m_mutex);
        auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
        systemByIdIndex.erase(systemId.systemId);
        auto& systemIndex = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
        systemIndex.erase(systemId.systemId);
    }
    completionHandler(dbResultToApiResult(dbResult));
}

nx::sql::DBResult SystemManager::shareSystem(
    nx::sql::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    NotificationCommand notificationCommand,
    data::AccountData* const inviteeAccount)
{
    bool isNewAccount = false;
    nx::sql::DBResult dbResult = fetchAccountToShareWith(
        queryContext, grantorEmail, sharing, inviteeAccount, &isNewAccount);
    if (dbResult != nx::sql::DBResult::ok)
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
    if (dbResult == nx::sql::DBResult::ok)
    {
        // Sharing already exists. Updating sharing.
        static_cast<api::SystemSharing&>(existingSharing) = sharing;
        return insertOrReplaceSharing(queryContext, std::move(existingSharing));
    }
    if (dbResult != nx::sql::DBResult::notFound)
    {
        NX_DEBUG(this, lm("Error fetching sharing (%1, %2) from Db")
            .arg(sharing.systemId).arg(inviteeAccount->email));
        return dbResult;
    }

    dbResult = addNewSharing(queryContext, *inviteeAccount, sharing);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = invokeSystemSharingExtension(
        &AbstractSystemSharingExtension::afterSharingSystem,
        queryContext,
        sharing,
        isNewAccount ? SharingType::invite : SharingType::sharingWithExistingAccount);
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Error invoking system sharing extension (%1, %2). %3")
            .arg(sharing.systemId).arg(inviteeAccount->email).arg(dbResult));
        return dbResult;
    }

    if (notificationCommand == NotificationCommand::sendNotification)
    {
        dbResult = notifyUserAboutNewSystem(
            queryContext, grantorEmail, *inviteeAccount, sharing);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::addNewSharing(
    nx::sql::QueryContext* const queryContext,
    const data::AccountData& inviteeAccount,
    const data::SystemSharing& sharing)
{
    api::SystemSharingEx sharingWithCalculatedData = createDerivedFromBase(sharing);
    sharingWithCalculatedData.lastLoginTime = nx::utils::utcTime();
    sharingWithCalculatedData.accountId = inviteeAccount.id;

    auto dbResult = m_systemSharingDao.calculateUsageFrequencyForANewSystem(
        queryContext,
        inviteeAccount.id,
        sharingWithCalculatedData.systemId,
        &sharingWithCalculatedData.usageFrequency);
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Error calculating usage frequency for sharing (%1, %2)")
            .arg(sharing.systemId).arg(inviteeAccount.email));
        return dbResult;
    }

    dbResult = insertOrReplaceSharing(
        queryContext,
        std::move(sharingWithCalculatedData));
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Error updating sharing (%1, %2) in Db")
            .arg(sharing.systemId).arg(inviteeAccount.email));
        return dbResult;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::deleteSharing(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const data::AccountData& inviteeAccount)
{
    const auto dbResult = m_systemSharingDao.deleteSharing(
        queryContext,
        systemId,
        {nx::sql::SqlFilterFieldEqual(
            "account_id", ":accountId", QnSql::serialized_field(inviteeAccount.id))});
    if (dbResult != nx::sql::DBResult::ok)
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

nx::sql::DBResult SystemManager::insertOrReplaceSharing(
    nx::sql::QueryContext* const queryContext,
    api::SystemSharingEx sharing)
{
    auto dbResult = m_systemSharingDao.insertOrReplaceSharing(queryContext, sharing);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, sharing = std::move(sharing)]() mutable
        {
            updateSharingInCache(std::move(sharing));
        });

    return nx::sql::DBResult::ok;
}

template<typename Notification>
nx::sql::DBResult SystemManager::fillSystemSharedNotification(
    nx::sql::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const std::string& systemId,
    const std::string& inviteeEmail,
    Notification* const notification)
{
    // TODO: #ak: Fetching system name from DB to ensure data consistency.
    data::SystemData system;
    auto dbResult = fetchSystemById(queryContext, systemId, &system);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    data::AccountData grantorAccount;
    dbResult = m_accountManager->fetchAccountByEmail(
        queryContext, grantorEmail, &grantorAccount);
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
            notification->message.sharer_name = grantorAccount.fullName;
            break;
        case nx::sql::DBResult::notFound:
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

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::notifyUserAboutNewSystem(
    nx::sql::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::AccountData& inviteeAccount,
    const api::SystemSharing& sharing)
{
    std::unique_ptr<AbstractNotification> notification;

    switch (inviteeAccount.statusCode)
    {
        case api::AccountStatus::invited:
        {
            auto inviteNotification = std::make_unique<InviteUserNotification>();
            auto dbResult = prepareInviteNotification(
                queryContext, grantorEmail, inviteeAccount,
                sharing.systemId, inviteNotification.get());
            if (dbResult != nx::sql::DBResult::ok)
                return dbResult;
            notification = std::move(inviteNotification);
            break;
        }

        default:
        {
            auto sharingNotification = std::make_unique<SystemSharedNotification>();
            auto dbResult = fillSystemSharedNotification(
                queryContext,
                grantorEmail, sharing.systemId, sharing.accountEmail,
                sharingNotification.get());
            if (dbResult != nx::sql::DBResult::ok)
                return dbResult;
            notification = std::move(sharingNotification);
            break;
        }
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this,
            notification = std::move(notification),
            locker = m_startedAsyncCallsCounter.getScopedIncrement()]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });
    return nx::sql::DBResult::ok;
}

boost::optional<std::vector<api::SystemDataEx>> SystemManager::selectSystemsFromCacheByFilter(
    const nx::utils::stree::AbstractResourceReader& requestResources,
    const data::DataFilter& filter)
{
    const auto accountEmail = requestResources.get<std::string>(nx::cloud::db::attr::authAccountEmail);

    auto systemId = requestResources.get<std::string>(nx::cloud::db::attr::authSystemId);
    if (!systemId)
        systemId = requestResources.get<std::string>(nx::cloud::db::attr::systemId);

    std::vector<api::SystemDataEx> resultData;

    QnMutexLocker lock(&m_mutex);

    const auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    if (systemId)
    {
        // Selecting system by id.
        auto systemIter = systemByIdIndex.find(systemId.get());
        if (systemIter == systemByIdIndex.end() || !filter.matches(*systemIter))
            return boost::none;
        resultData.emplace_back(*systemIter);
    }
    else if (accountEmail)
    {
        const auto& accountIndex = m_accountAccessRoleForSystem.get<kSharingByAccountEmail>();
        const auto accountSystemsRange = accountIndex.equal_range(accountEmail.get());
        for (auto it = accountSystemsRange.first; it != accountSystemsRange.second; ++it)
        {
            auto systemIter = systemByIdIndex.find(it->systemId);
            if (systemIter != systemByIdIndex.end() && filter.matches(*systemIter))
                resultData.push_back(*systemIter);
        }
    }
    else
    {
        // Filtering full system list.
        for (const auto& system: systemByIdIndex)
        {
            if (filter.matches(system))
                resultData.push_back(system);
        }
    }

    return resultData;
}

void SystemManager::addUserAccessInfo(
    const std::string& accountEmail,
    api::SystemDataEx& systemDataEx)
{
    const auto accountData =
        m_accountManager->findAccountByUserName(systemDataEx.ownerAccountEmail);
    if (accountData)
        systemDataEx.ownerFullName = accountData->fullName;
    const auto sharingData = getSystemSharingData(
        accountEmail,
        systemDataEx.id);
    if (sharingData && sharingData->isEnabled)
    {
        systemDataEx.accessRole = sharingData->accessRole;
        // Calculating system weight.
        systemDataEx.usageFrequency = nx::utils::calculateSystemUsageFrequency(
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

nx::sql::DBResult SystemManager::fetchAccountToShareWith(
    nx::sql::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    data::AccountData* const inviteeAccount,
    bool* const isNewAccount)
{
    *isNewAccount = false;

    nx::sql::DBResult dbResult = m_accountManager->fetchAccountByEmail(
        queryContext,
        sharing.accountEmail,
        inviteeAccount);
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
            return nx::sql::DBResult::ok;

        case nx::sql::DBResult::notFound:
            if (sharing.accessRole == api::SystemAccessRole::none)
            {
                // Removing sharing, no sense to create account.
                return nx::sql::DBResult::notFound;
            }
            break;

        default:
            NX_DEBUG(this, lm("Error fetching account by email %1. %2")
                .arg(sharing.accountEmail).arg(dbResult));
            return dbResult;
    }

    // Initializing account customization for case if account has to be created.
    data::SystemData system;
    dbResult = fetchSystemById(queryContext, sharing.systemId, &system);
    if (dbResult != nx::sql::DBResult::ok)
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

nx::sql::DBResult SystemManager::inviteNewUserToSystem(
    nx::sql::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId)
{
    nx::sql::DBResult dbResult =
        m_accountManager->insertAccount(queryContext, inviteeAccount);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    return scheduleInvintationNotificationDelivery(
        queryContext,
        inviterEmail,
        inviteeAccount,
        systemId);
}

nx::sql::DBResult SystemManager::scheduleInvintationNotificationDelivery(
    nx::sql::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId)
{
    auto notification = std::make_unique<InviteUserNotification>();
    auto dbResult = prepareInviteNotification(
        queryContext, inviterEmail, inviteeAccount, systemId, notification.get());
    if (dbResult != nx::sql::DBResult::ok)
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

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::prepareInviteNotification(
    nx::sql::QueryContext* const queryContext,
    const std::string& inviterEmail,
    const data::AccountData& inviteeAccount,
    const std::string& systemId,
    InviteUserNotification* const notification)
{
    data::AccountConfirmationCode accountConfirmationCode;
    nx::sql::DBResult dbResult = m_accountManager->createPasswordResetCode(
        queryContext,
        inviteeAccount.email,
        m_settings.accountManager().accountActivationCodeExpirationTimeout,
        &accountConfirmationCode);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = fillSystemSharedNotification(
        queryContext, inviterEmail, systemId, inviteeAccount.email, notification);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    notification->setActivationCode(std::move(accountConfirmationCode.code));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::updateSharingInDbAndGenerateTransaction(
    nx::sql::QueryContext* const queryContext,
    const std::string& grantorEmail,
    const data::SystemSharing& sharing,
    NotificationCommand notificationCommand)
{
    data::AccountData account;
    nx::sql::DBResult result = shareSystem(
        queryContext,
        grantorEmail,
        sharing,
        notificationCommand,
        &account);
    if (result != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Error sharing/unsharing system %1 with account %2. %3")
            .arg(sharing.systemId).arg(sharing.accountEmail)
            .arg(result));
        return result;
    }

    if (sharing.accessRole != api::SystemAccessRole::none)
    {
        result = generateSaveUserTransaction(queryContext, sharing, account);
        if (result != nx::sql::DBResult::ok)
            return result;

        result = generateUpdateFullNameTransaction(queryContext, sharing, account.fullName);
        if (result != nx::sql::DBResult::ok)
            return result;
    }
    else
    {
        result = generateRemoveUserTransaction(queryContext, sharing);
        if (result != nx::sql::DBResult::ok)
            return result;

        result = generateRemoveUserFullNameTransaction(queryContext, sharing);
        if (result != nx::sql::DBResult::ok)
            return result;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::generateSaveUserTransaction(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    const api::AccountData& account)
{
    vms::api::UserData userData;
    ec2::convert(sharing, &userData);
    userData.isCloud = true;
    userData.fullName = QString::fromStdString(account.fullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::SaveUser>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(userData));
}

nx::sql::DBResult SystemManager::generateUpdateFullNameTransaction(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    const std::string& newFullName)
{
    //generating "save full name" transaction
    nx::vms::api::ResourceParamWithRefData fullNameData;
    fullNameData.resourceId = QnUuid(sharing.vmsUserId.c_str());
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(newFullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::SetResourceParam>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(fullNameData));
}

nx::sql::DBResult SystemManager::generateRemoveUserTransaction(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharing& sharing)
{
    nx::vms::api::IdData userId;
    ec2::convert(sharing, &userId);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::RemoveUser>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(userId));
}

nx::sql::DBResult SystemManager::generateRemoveUserFullNameTransaction(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharing& sharing)
{
    nx::vms::api::ResourceParamWithRefData fullNameParam;
    fullNameParam.resourceId = QnUuid(sharing.vmsUserId.c_str());
    fullNameParam.name = Qn::USER_FULL_NAME;
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::RemoveResourceParam>(
            queryContext,
            sharing.systemId.c_str(),
            std::move(fullNameParam));
}

nx::sql::DBResult SystemManager::placeUpdateUserTransactionToEachSystem(
    nx::sql::QueryContext* const queryContext,
    const data::AccountUpdateDataWithEmail& accountUpdate)
{
    if (!accountUpdate.fullName)
        return nx::sql::DBResult::ok;

    std::vector<api::SystemSharingEx> sharings;
    auto dbResult = m_systemSharingDao.fetchUserSharingsByAccountEmail(
        queryContext, accountUpdate.email, &sharings);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;
    for (const auto& sharing: sharings)
    {
        dbResult = generateUpdateFullNameTransaction(
            queryContext, sharing, *accountUpdate.fullName);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;
    }

    return nx::sql::DBResult::ok;
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

nx::sql::DBResult SystemManager::updateSystem(
    nx::sql::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    nx::sql::DBResult dbResult = nx::sql::DBResult::ok;
    if (data.name)
    {
        dbResult = renameSystem(queryContext, data);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;
    }

    if (data.opaque)
    {
        dbResult = m_systemDao->execSystemOpaqueUpdate(queryContext, data);
        if (dbResult != nx::sql::DBResult::ok)
            return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, data]() mutable
        {
            updateSystemAttributesInCache(std::move(data));
        });

    return dbResult;
}

nx::sql::DBResult SystemManager::renameSystem(
    nx::sql::QueryContext* const queryContext,
    const data::SystemAttributesUpdate& data)
{
    NX_VERBOSE(this, lm("Renaming system %1 to %2")
        .arg(data.systemId).arg(data.name.get()));

    const auto result = m_systemDao->execSystemNameUpdate(queryContext, data);
    if (result != nx::sql::DBResult::ok)
        return result;

    // Generating transaction.
    nx::vms::api::ResourceParamWithRefData systemNameData;
    systemNameData.resourceId = QnUserResource::kAdminGuid;
    systemNameData.name = nx::settings_names::kNameSystemName;
    systemNameData.value = QString::fromStdString(data.name.get());

    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::SetResourceParam>(
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
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::sql::DBResult dbResult,
    data::SystemAttributesUpdate data,
    std::function<void(api::Result)> completionHandler)
{
    if (dbResult == nx::sql::DBResult::ok)
    {
        updateSystemAttributesInCache(std::move(data));
    }

    completionHandler(
        dbResult == nx::sql::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError);
}

template<typename SystemDictionary>
void SystemManager::activateSystemIfNeeded(
    const QnMutexLockerBase& /*lock*/,
    SystemDictionary& systemByIdIndex,
    typename SystemDictionary::iterator systemIter)
{
    if (systemIter->status == api::SystemStatus::activated &&
        !systemIter->activationInDbNeeded)
    {
        return;
    }

    systemByIdIndex.modify(
        systemIter,
        [](data::SystemData& system)
        {
            system.status = api::SystemStatus::activated;
            system.activationInDbNeeded = false;
        });

    using namespace std::placeholders;

    m_dbManager->executeUpdate<std::string /*systemId*/>(
        std::bind(&SystemManager::updateSystemStatus, this, _1, _2, api::SystemStatus::activated),
        systemIter->id,
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::sql::DBResult dbResult,
            std::string systemId)
        {
            updateSystemInCache(
                systemId,
                [dbResult](data::SystemData& system)
                {
                    if (dbResult == nx::sql::DBResult::ok)
                    {
                        system.status = api::SystemStatus::activated;
                        system.expirationTimeUtc = std::numeric_limits<int>::max();
                        system.activationInDbNeeded = false;
                    }
                    else
                    {
                        system.activationInDbNeeded = true;
                    }
                });
        });
}

template<typename Handler>
void SystemManager::updateSystemInCache(std::string systemId, Handler handler)
{
    QnMutexLocker lk(&m_mutex);

    auto& systemByIdIndex = m_systems.get<kSystemByIdIndex>();
    auto systemIter = systemByIdIndex.find(systemId);
    if (systemIter != systemByIdIndex.end())
    {
        systemByIdIndex.modify(
            systemIter,
            handler);
    }
}

void SystemManager::updateSystemStatusInCache(
    std::string systemId,
    api::SystemStatus systemStatus)
{
    updateSystemInCache(
        systemId,
        [systemStatus](data::SystemData& system)
        {
            system.status = systemStatus;
        });
}

nx::sql::DBResult SystemManager::saveUserSessionStart(
    nx::sql::QueryContext* queryContext,
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
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Error fetching user sharing (%1, %2). %3")
            .arg(*userSessionDescriptor.accountEmail)
            .arg(*userSessionDescriptor.systemId)
            .arg(dbResult));
        return dbResult;
    }

    // Calculating usage frequency.
    usageStatistics->lastloginTime = nx::utils::utcTime();
    usageStatistics->usageFrequency = nx::utils::calculateSystemUsageFrequency(
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
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_WARNING(this, lm("Error updating user login statistics"));
        return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, userSessionDescriptor, usageStatistics = *usageStatistics]()
        {
            updateSystemUsageStatisticsCache(
                userSessionDescriptor,
                usageStatistics);
        });

    return nx::sql::DBResult::ok;
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
            /*fallthrough*/
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

nx::sql::DBResult SystemManager::fillCache()
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Filling systems cache"));

    std::vector<data::SystemData> systems;
    auto result = doBlockingDbQuery(
        std::bind(&dao::AbstractSystemDataObject::fetchSystems, m_systemDao.get(),
            _1, nx::sql::InnerJoinFilterFields(), &systems));
    if (result != nx::sql::DBResult::ok)
        return result;

    NX_VERBOSE(this, lm("Fetched %1 systems").args(systems.size()));

    for (auto& system: systems)
    {
        auto idCopy = system.id;
        if (!m_systems.insert(std::move(system)).second)
        {
            NX_ASSERT(false);
        }
    }

    NX_VERBOSE(this, lm("Fetching account<->system relations"));

    result = doBlockingDbQuery(
        std::bind(&SystemManager::fetchSystemToAccountBinder, this, _1));
    if (result != nx::sql::DBResult::ok)
        return result;

    NX_VERBOSE(this, lm("Filled systems cache"));

    return nx::sql::DBResult::ok;
}

template<typename Func>
nx::sql::DBResult SystemManager::doBlockingDbQuery(Func func)
{
    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    m_dbManager->executeSelect(
        std::move(func),
        [&cacheFilledPromise](nx::sql::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });
    //waiting for completion
    return future.get();
}

nx::sql::DBResult SystemManager::fetchSystemToAccountBinder(
    nx::sql::QueryContext* queryContext)
{
    std::vector<api::SystemSharingEx> sharings;
    const auto result = m_systemSharingDao.fetchAllUserSharings(
        queryContext, &sharings);
    if (result != nx::sql::DBResult::ok)
    {
        NX_WARNING(this, lit("Failed to read system list from DB"));
        return nx::sql::DBResult::ioError;
    }

    for (auto& sharing: sharings)
    {
        sharing.usageFrequency -= 1.0;
        m_accountAccessRoleForSystem.emplace(std::move(sharing));
    }

    return nx::sql::DBResult::ok;
}

void SystemManager::dropExpiredSystems(uint64_t /*timerId*/)
{
    bool expected = false;
    if (!m_dropExpiredSystemsTaskStillRunning.compare_exchange_strong(expected, true))
    {
        NX_WARNING(this, lit("Previous 'drop expired systems' task is still running..."));
        return; //previous task is still running
    }

    using namespace std::placeholders;
    m_dbManager->executeUpdate(
        std::bind(&dao::AbstractSystemDataObject::deleteExpiredSystems, m_systemDao.get(), _1),
        std::bind(&SystemManager::expiredSystemsDeletedFromDb, this,
            m_startedAsyncCallsCounter.getScopedIncrement(), _1));
}

void SystemManager::expiredSystemsDeletedFromDb(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::sql::DBResult dbResult)
{
    if (dbResult == nx::sql::DBResult::ok)
    {
        //cleaning up systems cache
        QnMutexLocker lk(&m_mutex);
        auto& systemsByExpirationTime = m_systems.get<kSystemByExpirationTimeIndex>();
        for (auto systemIter = systemsByExpirationTime.lower_bound(nx::utils::timeSinceEpoch().count());
             systemIter != systemsByExpirationTime.end();
             )
        {
            NX_ASSERT(systemIter->status != api::SystemStatus::activated);

            auto& sharingBySystemId = m_accountAccessRoleForSystem.get<kSharingBySystemId>();
            sharingBySystemId.erase(systemIter->id);

            systemIter = systemsByExpirationTime.erase(systemIter);
        }
    }

    m_dropExpiredSystemsTaskStillRunning = false;
}

nx::sql::DBResult SystemManager::processEc2SaveUser(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    clusterdb::engine::Command<vms::api::UserData> transaction)
{
    const auto& vmsUser = transaction.params;

    NX_VERBOSE(this, lm("Processing vms transaction saveUser. "
        "user %1, systemId %2, permissions %3, enabled %4")
        .arg(vmsUser.email).arg(systemId)
        .arg(QnLexical::serialized(vmsUser.permissions)).arg(vmsUser.isEnabled));

    // Preparing SystemSharing structure.
    data::SystemSharing systemSharing;
    systemSharing.systemId = systemId;
    ec2::convert(vmsUser, &systemSharing);

    const nx::sql::InnerJoinFilterFields sqlFilter =
        {nx::sql::SqlFilterFieldEqual(
            "vms_user_id", ":vmsUserId",
            QnSql::serialized_field(transaction.historyAttributes.author.toSimpleString())),
         nx::sql::SqlFilterFieldEqual(
             "system_id", ":systemId", QnSql::serialized_field(systemId))};
    api::SystemSharingEx grantorInfo;
    auto dbResult = m_systemSharingDao.fetchSharing(
        queryContext,
        sqlFilter,
        &grantorInfo);
    if (dbResult != nx::sql::DBResult::ok &&
        dbResult != nx::sql::DBResult::notFound)
    {
        NX_DEBUG(this, lm("Error fetching saveUser transaction author. systemId %1")
            .arg(systemId));
        return dbResult;
    }
    //< System could have been shared by a local user, so we will not find him in our DB.

    // Updating db.
    data::AccountData account;
    dbResult = shareSystem(
        queryContext,
        grantorInfo.accountEmail,
        systemSharing,
        NotificationCommand::sendNotification,
        &account);
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Share system (%1) to %2 returned failed. %3")
            .arg(systemId).arg(systemSharing.accountEmail).arg(dbResult));
        return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, systemSharing = std::move(systemSharing)]() mutable
        {
            updateSharingInCache(std::move(systemSharing));
        });

    // Generating "save full name" transaction.
    nx::vms::api::ResourceParamWithRefData fullNameData;
    fullNameData.resourceId = vmsUser.id;
    fullNameData.name = Qn::USER_FULL_NAME;
    fullNameData.value = QString::fromStdString(account.fullName);
    return m_ec2SyncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <ec2::command::SetResourceParam>(
            queryContext,
            systemId,
            std::move(fullNameData));
}

nx::sql::DBResult SystemManager::processEc2RemoveUser(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    clusterdb::engine::Command<nx::vms::api::IdData> transaction)
{
    const auto& data = transaction.params;

    NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this), lm("Processing vms transaction removeUser. systemId %1, vms user id %2")
            .arg(systemId).arg(data.id));

    data::SystemSharing systemSharing;
    systemSharing.systemId = systemId;
    systemSharing.vmsUserId = data.id.toSimpleString().toStdString();

    const auto dbResult = m_systemSharingDao.deleteSharing(
        queryContext,
        systemId,
        {nx::sql::SqlFilterFieldEqual("vms_user_id", ":vmsUserId",
            QnSql::serialized_field(systemSharing.vmsUserId))});
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Failed to remove sharing by vms user id. system %1, vms user id %2. %3")
                .arg(systemId).arg(data.id)
                .arg(queryContext->connection()->lastErrorText()));
        return dbResult;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&SystemManager::removeVmsUserFromCache, this,
            systemSharing.systemId, systemSharing.vmsUserId));

    return nx::sql::DBResult::ok;
}

void SystemManager::removeVmsUserFromCache(
    const std::string& systemId,
    const std::string& vmsUserId)
{
    //updating "systems by account id" index
    QnMutexLocker lk(&m_mutex);

    auto& systemSharingBySystemIdAndVmsUserId =
        m_accountAccessRoleForSystem.get<kSharingBySystemIdAndVmsUserIdIndex>();
    systemSharingBySystemIdAndVmsUserId.erase(
        std::make_pair(systemId, vmsUserId));
}

nx::sql::DBResult SystemManager::processSetResourceParam(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    clusterdb::engine::Command<nx::vms::api::ResourceParamWithRefData> transaction)
{
    const auto& data = transaction.params;

    if (data.resourceId != QnUserResource::kAdminGuid ||
        data.name != nx::settings_names::kNameSystemName)
    {
        NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this), lm("Ignoring transaction setResourceParam with "
               "systemId %1, resourceId %2, param name %3, param value %4")
                .arg(systemId).arg(data.resourceId).arg(data.name).arg(data.value));
        return nx::sql::DBResult::ok;
    }

    data::SystemAttributesUpdate systemNameUpdate;
    systemNameUpdate.systemId = systemId;
    systemNameUpdate.name = data.value.toStdString();
    const auto dbResult = m_systemDao->execSystemNameUpdate(
        queryContext,
        systemNameUpdate);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&SystemManager::updateSystemAttributesInCache, this, systemNameUpdate));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManager::processRemoveResourceParam(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    clusterdb::engine::Command<nx::vms::api::ResourceParamWithRefData> data)
{
    // This can only be removal of already-removed user attribute.
    NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this), lm("Ignoring transaction %1 with "
            "systemId %2, resourceId %3, param name %4")
            .arg(::ec2::ApiCommand::toString(data.command)).arg(systemId)
            .arg(data.params.resourceId).arg(data.params.name));
    return nx::sql::DBResult::ok;
}

template<typename ExtensionFuncPtr, typename... Args>
nx::sql::DBResult SystemManager::invokeSystemSharingExtension(
    ExtensionFuncPtr extensionFunc,
    const Args&... args)
{
    for (auto& extension: m_systemSharingExtensions)
    {
        auto dbResult = (extension->*extensionFunc)(args...);
        if (dbResult != nx::sql::DBResult::ok)
        {
            NX_DEBUG(this, lm("Error invoking system sharing extension. %1")
                .arg(nx::sql::toString(dbResult)));
            return dbResult;
        }
    }

    return nx::sql::DBResult::ok;
}

} // namespace nx::cloud::db
