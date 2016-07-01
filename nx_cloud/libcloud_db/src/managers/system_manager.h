/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_system_manager_h
#define cloud_db_system_manager_h

#define BOOST_BIND_NO_PLACEHOLDERS

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/counter.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "access_control/abstract_authentication_data_provider.h"
#include "cache.h"
#include "data/data_filter.h"
#include "data/system_data.h"
#include "data_view.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

namespace conf
{
    class Settings;
}
class AccountManager;
class EventManager;

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
        const AccountManager& accountManager,
        const EventManager& eventManager,
        nx::db::AsyncSqlQueryExecutor* const dbManager) throw(std::runtime_error);
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
    void setSystemUserList(
        const AuthorizationInfo& authzInfo,
        data::SystemSharingList sharings,
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

    void updateSystemName(
        const AuthorizationInfo& authzInfo,
        data::SystemNameUpdate data,
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

    //!Create data view restricted by \a authzInfo and \a filter
    DataView<data::SystemData> createView(
        const AuthorizationInfo& authzInfo,
        data::DataFilter filter);
        
private:
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
        api::SystemSharing,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::identity<api::SystemSharing>>,
            //indexing by account
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                api::SystemSharing, std::string, &api::SystemSharing::accountEmail>>,
            //indexing by system
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                api::SystemSharing, std::string, &api::SystemSharing::systemID>>
        >
    > AccountSystemAccessRoleDict;

    constexpr static const int kSharingUniqueIndex = 0;
    constexpr static const int kSharingByAccountEmail = 1;
    constexpr static const int kSharingBySystemId = 2;

    const conf::Settings& m_settings;
    nx::utils::TimerManager* const m_timerManager;
    const AccountManager& m_accountManager;
    const EventManager& m_eventManager;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    //!map<id, system>
    SystemsDict m_systems;
    mutable QnMutex m_mutex;
    AccountSystemAccessRoleDict m_accountAccessRoleForSystem;
    QnCounter m_startedAsyncCallsCounter;
    uint64_t m_dropSystemsTimerId;
    std::atomic<bool> m_dropExpiredSystemsTaskStillRunning;

    nx::db::DBResult insertSystemToDB(
        QSqlDatabase* const connection,
        const data::SystemRegistrationDataWithAccount& newSystem,
        data::SystemData* const systemData);
    void systemAdded(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccount systemRegistrationData,
        data::SystemData systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    nx::db::DBResult insertSystemSharingToDB(
        QSqlDatabase* const connection,
        const data::SystemSharing& systemSharing);
    void systemSharingAdded(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemSharing sytemSharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult markSystemAsDeleted(
        QSqlDatabase* const connection,
        const std::string& systemId);
    void systemMarkedAsDeleted(
        QnCounter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult deleteSystemFromDB(
        QSqlDatabase* const connection,
        const data::SystemID& systemID);
    void systemDeleted(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemID systemID,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult updateSharingInDB(
        QSqlDatabase* const connection,
        const data::SystemSharing& sharing);
    void sharingUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult updateUserListInDB(
        QSqlDatabase* const connection,
        const std::string& systemId,
        const data::SystemSharingList& sharingList);
    void userListUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        const std::string& systemId,
        data::SystemSharingList sharingList,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult updateSystemNameInDB(
        QSqlDatabase* const connection,
        const data::SystemNameUpdate& data);
    void systemNameUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemNameUpdate data,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult activateSystem(
        QSqlDatabase* const connection,
        const std::string& systemId);
    void systemActivated(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        std::string systemId,
        std::function<void(api::ResultCode)> completionHandler);

    /** returns sharing permissions depending on current access role */
    api::SystemAccessRoleList getSharingPermissions(
        api::SystemAccessRole accessRole) const;

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchSystems(QSqlDatabase* connection, int* const /*dummy*/);
    nx::db::DBResult fetchSystemToAccountBinder(
        QSqlDatabase* connection,
        int* const /*dummy*/);

    void dropExpiredSystems(uint64_t timerId);
    nx::db::DBResult deleteExpiredSystemsFromDb(QSqlDatabase* connection);
    void expiredSystemsDeletedFromDb(
        QnCounter::ScopedIncrement /*asyncCallLocker*/,
        nx::db::DBResult dbResult);
};

}   //cdb
}   //nx

#endif
