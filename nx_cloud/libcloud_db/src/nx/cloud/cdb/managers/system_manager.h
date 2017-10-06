#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx_ec/data/api_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>
#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/filter.h>

#include "account_manager.h"
#include "cache.h"
#include "data_view.h"
#include "managers_types.h"
#include "system_sharing_manager.h"
#include "../access_control/auth_types.h"
#include "../access_control/abstract_authentication_data_provider.h"
#include "../dao/rdb/system_sharing_data_object.h"
#include "../dao/abstract_system_data_object.h"
#include "../data/account_data.h"
#include "../data/data_filter.h"
#include "../data/system_data.h"
#include "../ec2/transaction_log.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; } // namespace conf

class AbstractEmailManager;
class AbstractAccountManager;
class AbstractSystemHealthInfoProvider;

namespace ec2 {

class SyncronizationEngine;

} // namespace ec2

class InviteUserNotification;

class AbstractSystemManager
{
public:
    virtual ~AbstractSystemManager() = default;

    virtual boost::optional<api::SystemData> findSystemById(const std::string& id) const = 0;
};

/**
 * Provides methods for manipulating system data on persisent storage.
 * Calls DBManager instance to perform DB manipulation.
 * @note All data can be cached.
 */
class SystemManager:
    public AbstractSystemManager,
    public AbstractSystemSharingManager,
    public AbstractAuthenticationDataProvider,
    public AbstractAccountManagerExtension
{
public:
    enum class NotificationCommand
    {
        sendNotification,
        doNotSendNotification,
    };

    /**
     * Fills internal cache.
     * @throw std::runtime_error In case of failure to pre-fill data cache.
     */
    SystemManager(
        const conf::Settings& settings,
        nx::utils::StandaloneTimerManager* const timerManager,
        AbstractAccountManager* const accountManager,
        const AbstractSystemHealthInfoProvider& systemHealthInfoProvider,
        nx::utils::db::AsyncSqlQueryExecutor* const dbManager,
        AbstractEmailManager* const emailManager,
        ec2::SyncronizationEngine* const ec2SyncronizationEngine) noexcept(false);
    virtual ~SystemManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    /** Binds system to an account associated with authzInfo. */
    void bindSystemToAccount(
        const AuthorizationInfo& authzInfo,
        data::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);
    void unbindSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode)> completionHandler);
    /**
     * Fetches systems satisfying specified filter.
     * @param eventReceiver Events related to returned data are reported to this functor
     * @note It is garanteed that events are delivered after completionHandler has returned and only if request was a success.
     * @note If filter is empty, all systems authorized for authInfo are returned.
     * @note if request can be completed immediately (e.g., data is present in internal cache) completionHandler will be invoked within this call.
     */
    void getSystems(
        const AuthorizationInfo& authzInfo,
        data::DataFilter filter,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler);
    void shareSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler);
    /** Provides list of cloud accounts who have access to this system. */
    void getCloudUsersOfSystem(
        const AuthorizationInfo& authzInfo,
        const data::DataFilter& filter,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler);

    void getAccessRoleList(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler);

    void updateSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemAttributesUpdate data,
        std::function<void(api::ResultCode)> completionHandler);

    void recordUserSessionStart(
        const AuthorizationInfo& authzInfo,
        data::UserSessionDescriptor userSessionDescriptor,
        std::function<void(api::ResultCode)> completionHandler);

    virtual boost::optional<api::SystemData> findSystemById(const std::string& id) const override;

    virtual api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemId) const override;
    virtual boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemId) const override;

    std::vector<api::SystemSharingEx> fetchAllSharings() const;

    //---------------------------------------------------------------------------------------------
    // Events.

    nx::utils::Subscription<std::string>& systemMarkedAsDeletedSubscription();

    virtual void addSystemSharingExtension(AbstractSystemSharingExtension* extension) override;
    virtual void removeSystemSharingExtension(AbstractSystemSharingExtension* extension) override;

private:
    static std::pair<std::string, std::string> extractSystemIdAndVmsUserId(
        const api::SystemSharing& data);
    static std::pair<std::string, std::string> extractSystemIdAndAccountEmail(
        const api::SystemSharing& data);

    typedef boost::multi_index::multi_index_container<
        data::SystemData,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::member<
                    api::SystemData, std::string, &api::SystemData::id>>,
            //indexing by expiration time
            boost::multi_index::ordered_non_unique<
                boost::multi_index::member<
                    data::SystemData, int, &data::SystemData::expirationTimeUtc>,
                std::greater<int>>
        >
    > SystemsDict;

    constexpr static const int kSystemByIdIndex = 0;
    constexpr static const int kSystemByExpirationTimeIndex = 1;

    typedef boost::multi_index::multi_index_container<
        api::SystemSharingEx,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::identity<api::SystemSharing>>,
            //indexing by account
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                api::SystemSharing, std::string, &api::SystemSharing::accountEmail>>,
            //indexing by system
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                api::SystemSharing, std::string, &api::SystemSharing::systemId>>,
            //indexing by pair<systemId, vmsUserId>
            boost::multi_index::ordered_non_unique<boost::multi_index::global_fun<
                const api::SystemSharing&,
                std::pair<std::string, std::string>,
                &SystemManager::extractSystemIdAndVmsUserId>>,
            //indexing by pair<systemId, accountEmail>
            boost::multi_index::ordered_non_unique<boost::multi_index::global_fun<
                const api::SystemSharing&,
                std::pair<std::string, std::string>,
                &SystemManager::extractSystemIdAndAccountEmail>>
        >
    > AccountSystemAccessRoleDict;

    struct InsertNewSystemToDbResult
    {
        data::SystemData systemData;
        data::SystemSharing ownerSharing;
    };

    struct SaveUserSessionResult
    {
        float usageFrequency;
        std::chrono::system_clock::time_point lastloginTime;
    };

    constexpr static const int kSharingUniqueIndex = 0;
    constexpr static const int kSharingByAccountEmail = 1;
    constexpr static const int kSharingBySystemId = 2;
    constexpr static const int kSharingBySystemIdAndVmsUserIdIndex = 3;
    constexpr static const int kSharingBySystemIdAndAccountEmailIndex = 4;

    const conf::Settings& m_settings;
    nx::utils::StandaloneTimerManager* const m_timerManager;
    AbstractAccountManager* const m_accountManager;
    const AbstractSystemHealthInfoProvider& m_systemHealthInfoProvider;
    nx::utils::db::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractEmailManager* const m_emailManager;
    ec2::SyncronizationEngine* const m_ec2SyncronizationEngine;
    SystemsDict m_systems;
    mutable QnMutex m_mutex;
    AccountSystemAccessRoleDict m_accountAccessRoleForSystem;
    nx::utils::Counter m_startedAsyncCallsCounter;
    uint64_t m_dropSystemsTimerId;
    std::atomic<bool> m_dropExpiredSystemsTaskStillRunning;
    nx::utils::Subscription<std::string> m_systemMarkedAsDeletedSubscription;
    std::unique_ptr<dao::AbstractSystemDataObject> m_systemDao;
    dao::rdb::SystemSharingDataObject m_systemSharingDao;
    std::set<AbstractSystemSharingExtension*> m_systemSharingExtensions;

    virtual void afterUpdatingAccount(
        nx::utils::db::QueryContext*,
        const data::AccountUpdateDataWithEmail&) override;

    nx::utils::db::DBResult insertSystemToDB(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemRegistrationDataWithAccount& newSystem,
        InsertNewSystemToDbResult* const result);
    nx::utils::db::DBResult insertNewSystemDataToDb(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemRegistrationDataWithAccount& newSystem,
        InsertNewSystemToDbResult* const result);
    nx::utils::db::DBResult insertOwnerSharingToDb(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const data::SystemRegistrationDataWithAccount& newSystem,
        nx::cdb::data::SystemSharing* const ownerSharing);
    void systemAdded(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccount systemRegistrationData,
        InsertNewSystemToDbResult systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    nx::utils::db::DBResult markSystemAsDeleted(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId);
    void systemMarkedAsDeleted(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);
    void markSystemAsDeletedInCache(const std::string& systemId);

    nx::utils::db::DBResult deleteSystemFromDB(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemId& systemId);
    void systemDeleted(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemId systemId,
        std::function<void(api::ResultCode)> completionHandler);

    //---------------------------------------------------------------------------------------------
    // System sharing methods. TODO: #ak: move to a separate class

    /**
     * @param inviteeAccount Filled with attributes of account sharing.accountEmail.
     */
    nx::utils::db::DBResult shareSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        NotificationCommand notificationCommand,
        data::AccountData* const inviteeAccount);
    nx::utils::db::DBResult addNewSharing(
        nx::utils::db::QueryContext* const queryContext,
        const data::AccountData& inviteeAccount,
        const data::SystemSharing& sharing);
    nx::utils::db::DBResult deleteSharing(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const data::AccountData& inviteeAccount);

    nx::utils::db::DBResult insertOrReplaceSharing(
        nx::utils::db::QueryContext* const queryContext,
        api::SystemSharingEx sharing);

    template<typename Notification>
    nx::utils::db::DBResult fillSystemSharedNotification(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const std::string& systemId,
        const std::string& inviteeEmail,
        Notification* const notification);

    nx::utils::db::DBResult notifyUserAboutNewSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::AccountData& inviteeAccount,
        const api::SystemSharing& sharing);

    /**
     * TODO: #ak Having both requestResources and filter looks overabundant.
     * @return boost::none is returned in case if distinct filter has been specified 
     *   and system was not found with this filter.
     */
    boost::optional<std::vector<api::SystemDataEx>> selectSystemsFromCacheByFilter(
        const nx::utils::stree::AbstractResourceReader& requestResources,
        const data::DataFilter& filter);
    void addUserAccessInfo(const std::string& accountEmail, api::SystemDataEx& systemDataEx);

    /**
     * Fetch existing account or create a new one sending corresponding notification.
     */
    nx::utils::db::DBResult fetchAccountToShareWith(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        data::AccountData* const inviteeAccount,
        bool* const isNewAccount);
    nx::utils::db::DBResult inviteNewUserToSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId);
    nx::utils::db::DBResult scheduleInvintationNotificationDelivery(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId);
    nx::utils::db::DBResult prepareInviteNotification(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId,
        InviteUserNotification* const notification);
    nx::utils::db::DBResult updateSharingInDbAndGenerateTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        NotificationCommand notificationCommand);

    nx::utils::db::DBResult generateSaveUserTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        const api::AccountData& account);
    nx::utils::db::DBResult generateUpdateFullNameTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        const std::string& newFullName);
    nx::utils::db::DBResult generateRemoveUserTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing);
    nx::utils::db::DBResult generateRemoveUserFullNameTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing);

    nx::utils::db::DBResult placeUpdateUserTransactionToEachSystem(
        nx::utils::db::QueryContext* const queryContext,
        const data::AccountUpdateDataWithEmail& accountData);
    void updateSharingInCache(data::SystemSharing sharing);
    void updateSharingInCache(
        api::SystemSharingEx sharing,
        bool updateExFields = true);

    nx::utils::db::DBResult updateSystem(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);
    nx::utils::db::DBResult renameSystem(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);
    void updateSystemAttributesInCache(
        data::SystemAttributesUpdate data);
    void systemNameUpdated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemAttributesUpdate data,
        std::function<void(api::ResultCode)> completionHandler);

    template<typename SystemDictionary>
    void activateSystemIfNeeded(
        const QnMutexLockerBase& lock,
        SystemDictionary& systemByIdIndex,
        typename SystemDictionary::iterator systemIter);
    void systemActivated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        std::string systemId);

    nx::utils::db::DBResult saveUserSessionStart(
        nx::utils::db::QueryContext* queryContext,
        const data::UserSessionDescriptor& userSessionDescriptor,
        SaveUserSessionResult* const result);
    void updateSystemUsageStatisticsCache(
        const data::UserSessionDescriptor& userSessionDescriptor,
        const SaveUserSessionResult& usageStatistics);

    /**
     * @return Sharing permissions depending on current access role.
     */
    api::SystemAccessRoleList getSharingPermissions(
        api::SystemAccessRole accessRole) const;

    nx::utils::db::DBResult fillCache();
    template<typename Func> nx::utils::db::DBResult doBlockingDbQuery(Func func);
    nx::utils::db::DBResult fetchSystemById(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        data::SystemData* const system);
    nx::utils::db::DBResult fetchSystemToAccountBinder(
        nx::utils::db::QueryContext* queryContext);

    void dropExpiredSystems(uint64_t timerId);
    void expiredSystemsDeletedFromDb(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult);

    /** Processes saveUser transaction received from mediaserver. */
    nx::utils::db::DBResult processEc2SaveUser(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiUserData> data,
        data::SystemSharing* const systemSharingData);
    void onEc2SaveUserDone(
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::utils::db::DBResult processEc2RemoveUser(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiIdData> data,
        data::SystemSharing* const systemSharingData);
    void onEc2RemoveUserDone(
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::utils::db::DBResult processSetResourceParam(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> data,
        data::SystemAttributesUpdate* const systemNameUpdate);
    void onEc2SetResourceParamDone(
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult,
        data::SystemAttributesUpdate systemNameUpdate);

    nx::utils::db::DBResult processRemoveResourceParam(
        nx::utils::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> data);
    void onEc2RemoveResourceParamDone(
        nx::utils::db::QueryContext* /*queryContext*/,
        nx::utils::db::DBResult dbResult);

    template<typename ExtensionFuncPtr, typename... Args>
    nx::utils::db::DBResult invokeSystemSharingExtension(
        ExtensionFuncPtr extensionFunc,
        const Args&... args);
};

} // namespace cdb
} // namespace nx
