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
#include <utils/db/async_sql_query_executor.h>
#include <utils/db/filter.h>

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

namespace conf {

class Settings;

} // namespace conf

class AbstractEmailManager;
class AccountManager;
class SystemHealthInfoProvider;

namespace ec2 {

class SyncronizationEngine;

} // namespace ec2

class InviteUserNotification;

/**
 * Provides methods for manipulating system data on persisent storage.
 * Calls DBManager instance to perform DB manipulation.
 * @note All data can be cached.
 */
class SystemManager:
    public AbstractSystemSharingManager,
    public AbstractAuthenticationDataProvider
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
        AccountManager* const accountManager,
        const SystemHealthInfoProvider& systemHealthInfoProvider,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
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

    /**
     * @return api::SystemAccessRole::none is returned if
     * - accountEmail has no rights for systemId
     * - accountEmail or systemId is unknown
     */
    virtual api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemId) const override;
    virtual boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemId) const override;

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
    AccountManager* const m_accountManager;
    const SystemHealthInfoProvider& m_systemHealthInfoProvider;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
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

    nx::db::DBResult insertSystemToDB(
        nx::db::QueryContext* const queryContext,
        const data::SystemRegistrationDataWithAccount& newSystem,
        InsertNewSystemToDbResult* const result);
    nx::db::DBResult insertNewSystemDataToDb(
        nx::db::QueryContext* const queryContext,
        const data::SystemRegistrationDataWithAccount& newSystem,
        InsertNewSystemToDbResult* const result);
    nx::db::DBResult insertOwnerSharingToDb(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const data::SystemRegistrationDataWithAccount& newSystem,
        nx::cdb::data::SystemSharing* const ownerSharing);
    void systemAdded(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccount systemRegistrationData,
        InsertNewSystemToDbResult systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    nx::db::DBResult markSystemAsDeleted(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);
    void systemMarkedAsDeleted(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);
    void markSystemAsDeletedInCache(const std::string& systemId);

    nx::db::DBResult deleteSystemFromDB(
        nx::db::QueryContext* const queryContext,
        const data::SystemId& systemId);
    void systemDeleted(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemId systemId,
        std::function<void(api::ResultCode)> completionHandler);

    //---------------------------------------------------------------------------------------------
    // System sharing methods. TODO: #ak: move to a separate class

    /**
     * @param inviteeAccount Filled with attributes of account sharing.accountEmail.
     */
    nx::db::DBResult shareSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        NotificationCommand notificationCommand,
        data::AccountData* const inviteeAccount);
    nx::db::DBResult addNewSharing(
        nx::db::QueryContext* const queryContext,
        const data::AccountData& inviteeAccount,
        const data::SystemSharing& sharing);
    nx::db::DBResult deleteSharing(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const data::AccountData& inviteeAccount);

    nx::db::DBResult insertOrReplaceSharing(
        nx::db::QueryContext* const queryContext,
        api::SystemSharingEx sharing);

    template<typename Notification>
    nx::db::DBResult fillSystemSharedNotification(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const std::string& systemId,
        const std::string& inviteeEmail,
        Notification* const notification);

    nx::db::DBResult notifyUserAboutNewSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::AccountData& inviteeAccount,
        const api::SystemSharing& sharing);

    /**
     * Fetch existing account or create a new one sending corresponding notification.
     */
    nx::db::DBResult fetchAccountToShareWith(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        data::AccountData* const inviteeAccount,
        bool* const isNewAccount);
    nx::db::DBResult inviteNewUserToSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId);
    nx::db::DBResult scheduleInvintationNotificationDelivery(
        nx::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId);
    nx::db::DBResult prepareInviteNotification(
        nx::db::QueryContext* const queryContext,
        const std::string& inviterEmail,
        const data::AccountData& inviteeAccount,
        const std::string& systemId,
        InviteUserNotification* const notification);
    nx::db::DBResult updateSharingInDbAndGenerateTransaction(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        NotificationCommand notificationCommand);

    nx::db::DBResult generateSaveUserTransaction(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        const api::AccountData& account);
    nx::db::DBResult generateUpdateFullNameTransaction(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        const std::string& newFullName);
    nx::db::DBResult generateRemoveUserTransaction(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing);
    nx::db::DBResult generateRemoveUserFullNameTransaction(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing);

    nx::db::DBResult placeUpdateUserTransactionToEachSystem(
        nx::db::QueryContext* const queryContext,
        const data::AccountUpdateDataWithEmail& accountData);
    void updateSharingInCache(data::SystemSharing sharing);
    void updateSharingInCache(
        api::SystemSharingEx sharing,
        bool updateExFields = true);

    nx::db::DBResult updateSystem(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);
    nx::db::DBResult renameSystem(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);
    void updateSystemAttributesInCache(
        data::SystemAttributesUpdate data);
    void systemNameUpdated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemAttributesUpdate data,
        std::function<void(api::ResultCode)> completionHandler);

    template<typename SystemDictionary>
    void activateSystemIfNeeded(
        const QnMutexLockerBase& lock,
        SystemDictionary& systemByIdIndex,
        typename SystemDictionary::iterator systemIter);
    void systemActivated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        std::string systemId);

    nx::db::DBResult saveUserSessionStart(
        nx::db::QueryContext* queryContext,
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

    nx::db::DBResult fillCache();
    template<typename Func> nx::db::DBResult doBlockingDbQuery(Func func);
    nx::db::DBResult fetchSystemById(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        data::SystemData* const system);
    nx::db::DBResult fetchSystemToAccountBinder(
        nx::db::QueryContext* queryContext);

    void dropExpiredSystems(uint64_t timerId);
    void expiredSystemsDeletedFromDb(
        nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult);

    /** Processes saveUser transaction received from mediaserver. */
    nx::db::DBResult processEc2SaveUser(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiUserData> data,
        data::SystemSharing* const systemSharingData);
    void onEc2SaveUserDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::db::DBResult processEc2RemoveUser(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiIdData> data,
        data::SystemSharing* const systemSharingData);
    void onEc2RemoveUserDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::db::DBResult processSetResourceParam(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> data,
        data::SystemAttributesUpdate* const systemNameUpdate);
    void onEc2SetResourceParamDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemAttributesUpdate systemNameUpdate);

    nx::db::DBResult processRemoveResourceParam(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::QnTransaction<::ec2::ApiResourceParamWithRefData> data);
    void onEc2RemoveResourceParamDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult);

    template<typename ExtensionFuncPtr, typename... Args>
    nx::db::DBResult invokeSystemSharingExtension(
        ExtensionFuncPtr extensionFunc,
        const Args&... args);
};

} // namespace cdb
} // namespace nx
