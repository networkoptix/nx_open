#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
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
#include <utils/common/counter.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "access_control/abstract_authentication_data_provider.h"
#include "cache.h"
#include "data/account_data.h"
#include "data/data_filter.h"
#include "data/system_data.h"
#include "data_view.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

namespace conf {
class Settings;
}   // namespace conf

class AccountManager;
class SystemHealthInfoProvider;

namespace ec2 {
class IncomingTransactionDispatcher;
class TransactionLog;
}   // namespace ec2 

/*!
    Provides methods for manipulating system data on persisent storage.
    Calls \a DBManager instance to perform DB manipulation. SQL requests are written in this class
    \note All data can be cached
*/
class SystemManager
:
    public AbstractAuthenticationDataProvider
{
public:
    /*!
        Fills internal cache
        \throw std::runtime_error In case of failure to pre-fill data cache
    */
    SystemManager(
        const conf::Settings& settings,
        nx::utils::TimerManager* const timerManager,
        AccountManager* const accountManager,
        const SystemHealthInfoProvider& systemHealthInfoProvider,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
        ec2::TransactionLog* const transactionLog,
        ec2::IncomingTransactionDispatcher* const transactionDispatcher) throw(std::runtime_error);
    virtual ~SystemManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const stree::AbstractResourceReader& authSearchInputData,
        stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    //!Binds system to an account associated with \a authzInfo
    void bindSystemToAccount(
        const AuthorizationInfo& authzInfo,
        data::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);
    void unbindSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemID systemID,
        std::function<void(api::ResultCode)> completionHandler);
    //!Fetches systems satisfying specified \a filter
    /*!
        \param eventReceiver Events related to returned data are reported to this functor
        \note It is garanteed that events are delivered after \a completionHandler has returned and only if request was a success
        \note If \a filter is empty, all systems authorized for \a authInfo are returned
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
    */
    void getSystems(
        const AuthorizationInfo& authzInfo,
        data::DataFilter filter,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler );
    void shareSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler);
    //!Provides list of cloud accounts who have access to this system
    void getCloudUsersOfSystem(
        const AuthorizationInfo& authzInfo,
        const data::DataFilter& filter,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler);

    void getAccessRoleList(
        const AuthorizationInfo& authzInfo,
        data::SystemID systemID,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler);

    void renameSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemNameUpdate data,
        std::function<void(api::ResultCode)> completionHandler);
    
    void recordUserSessionStart(
        const AuthorizationInfo& authzInfo,
        data::UserSessionDescriptor userSessionDescriptor,
        std::function<void(api::ResultCode)> completionHandler);

    //void addSubscription(
    //    const AuthorizationInfo& authzInfo,
    //    const std::string& systemID,
    //    const std::string& productID,
    //    std::function<void(api::ResultCode)> completionHandler);
    ///*!
    //    \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
    //*/
    //void getActiveSubscriptions(
    //    const AuthorizationInfo& authzInfo,
    //    const std::string& systemID,
    //    std::function<void(api::ResultCode, std::vector<data::SubscriptionData>)> completionHandler);

    /*!
        \return \a api::SystemAccessRole::none is returned if\n
        - \a accountEmail has no rights for \a systemID
        - \a accountEmail or \a systemID is unknown
    */
    api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemID) const;
    boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemID) const;

    //!Create data view restricted by \a authzInfo and \a filter
    DataView<data::SystemData> createView(
        const AuthorizationInfo& authzInfo,
        data::DataFilter filter);
        
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
                api::SystemSharing, std::string, &api::SystemSharing::systemID>>,
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
    nx::utils::TimerManager* const m_timerManager;
    AccountManager* const m_accountManager;
    const SystemHealthInfoProvider& m_systemHealthInfoProvider;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    ec2::TransactionLog* const m_transactionLog;
    ec2::IncomingTransactionDispatcher* const m_transactionDispatcher;
    //!map<id, system>
    SystemsDict m_systems;
    mutable QnMutex m_mutex;
    AccountSystemAccessRoleDict m_accountAccessRoleForSystem;
    QnCounter m_startedAsyncCallsCounter;
    uint64_t m_dropSystemsTimerId;
    std::atomic<bool> m_dropExpiredSystemsTaskStillRunning;

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
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccount systemRegistrationData,
        InsertNewSystemToDbResult systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    //nx::db::DBResult insertSystemSharingToDB(
    //    nx::db::QueryContext* const queryContext,
    //    const data::SystemSharing& systemSharing);
    void systemSharingAdded(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sytemSharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult markSystemAsDeleted(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);
    void systemMarkedAsDeleted(
        QnCounter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult deleteSystemFromDB(
        nx::db::QueryContext* const queryContext,
        const data::SystemID& systemID);
    void systemDeleted(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemID systemID,
        std::function<void(api::ResultCode)> completionHandler);

    /**
     * @param targetAccountData Filled with attributes of account \a sharing.accountEmail
     */
    nx::db::DBResult updateSharingInDb(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        data::AccountData* const targetAccountData);
    /**
     * Fetches existing account or creates new one sending corresponding notification.
     */
    nx::db::DBResult fetchAccountToShareWith(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing,
        data::AccountData* const targetAccountData);
    /**
     * New system usage frequency is calculated as max(usage frequency of all account's systems) + 1.
     */
    nx::db::DBResult calculateUsageFrequencyForANewSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& accountId,
        const std::string& systemId,
        float* const newSystemInitialUsageFrequency);
    nx::db::DBResult updateSharingInDbAndGenerateTransaction(
        nx::db::QueryContext* const queryContext,
        const std::string& grantorEmail,
        const data::SystemSharing& sharing);
    void sharingUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing,
        std::function<void(api::ResultCode)> completionHandler);
    void updateSharingInCache(data::SystemSharing sharing);
    void updateSharingInCache(
        api::SystemSharingEx sharing,
        bool updateExFields = true);

    nx::db::DBResult updateSystemNameInDB(
        nx::db::QueryContext* const queryContext,
        const data::SystemNameUpdate& data);
    nx::db::DBResult execSystemNameUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemNameUpdate& data);
    void updateSystemNameInCache(
        data::SystemNameUpdate data);
    void systemNameUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemNameUpdate data,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult activateSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);
    void systemActivated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult saveUserSessionStartToDb(
        nx::db::QueryContext* queryContext,
        const data::UserSessionDescriptor& userSessionDescriptor,
        SaveUserSessionResult* const result);
    void userSessionStartSavedToDb(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::UserSessionDescriptor userSessionDescriptor,
        SaveUserSessionResult result,
        std::function<void(api::ResultCode)> completionHandler);

    /** returns sharing permissions depending on current access role */
    api::SystemAccessRoleList getSharingPermissions(
        api::SystemAccessRole accessRole) const;

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchSystems(nx::db::QueryContext* queryContext, int* const /*dummy*/);
    nx::db::DBResult fetchSystemToAccountBinder(
        nx::db::QueryContext* queryContext,
        int* const /*dummy*/);

    void dropExpiredSystems(uint64_t timerId);
    nx::db::DBResult deleteExpiredSystemsFromDb(nx::db::QueryContext* queryContext);
    void expiredSystemsDeletedFromDb(
        QnCounter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult);

    /** Processes saveUser transaction received from mediaserver */
    nx::db::DBResult processEc2SaveUser(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::ApiUserData data,
        data::SystemSharing* const systemSharingData);
    void onEc2SaveUserDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::db::DBResult processEc2RemoveUser(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::ApiIdData data,
        data::SystemSharing* const systemSharingData);
    void onEc2RemoveUserDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing);

    nx::db::DBResult processSetResourceParam(
        nx::db::QueryContext* queryContext,
        const nx::String& systemId,
        ::ec2::ApiResourceParamWithRefData data,
        data::SystemNameUpdate* const systemNameUpdate);
    void onEc2SetResourceParamDone(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult dbResult,
        data::SystemNameUpdate systemNameUpdate);

    static float calculateSystemUsageFrequency(
        std::chrono::system_clock::time_point lastLoginTime,
        float currentUsageFrequency);
};

}   //cdb
}   //nx
