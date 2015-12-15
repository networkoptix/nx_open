/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_system_manager_h
#define cloud_db_system_manager_h

#define BOOST_BIND_NO_PLACEHOLDERS

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <utils/db/db_manager.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_safe_counter.h>

#include "access_control/auth_types.h"
#include "access_control/abstract_authentication_data_provider.h"
#include "cache.h"
#include "data/data_filter.h"
#include "data/system_data.h"
#include "data_view.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

class AccountManager;

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
        const AccountManager& accountManager,
        nx::db::DBManager* const dbManager) throw(std::runtime_error);
    virtual ~SystemManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        stree::AbstractResourceWriter* const authProperties,
        std::function<void(bool)> completionHandler) override;

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
        std::function<void(api::ResultCode, data::SystemDataList)> completionHandler );
    //!Share system with specified account. Operation allowed for system owner and editor_with_sharing only
    void shareSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler);
    //!Provides list of cloud accounts who have access to this system
    void getCloudUsersOfSystem(
        const AuthorizationInfo& authzInfo,
        const data::DataFilter& filter,
        std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler);
    void updateSharing(
        const AuthorizationInfo& authzInfo,
        data::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler);

    void addSubscription(
        const AuthorizationInfo& authzInfo,
        const QnUuid& systemID,
        const QnUuid& productID,
        std::function<void(api::ResultCode)> completionHandler);
    /*!
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
    */
    void getActiveSubscriptions(
        const AuthorizationInfo& authzInfo,
        const QnUuid& systemID,
        std::function<void(api::ResultCode, std::vector<data::SubscriptionData>)> completionHandler);

    boost::optional<data::SystemData> findSystemByID(const QnUuid& id) const;
    /*!
        \return \a api::SystemAccessRole::none is returned if\n
        - \a accountEmail has no rights for \a systemID
        - \a accountEmail or \a systemID is unknown
    */
    api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail, const QnUuid& systemID) const;

    //!Create data view restricted by \a authzInfo and \a filter
    DataView<data::SystemData> createView(
        const AuthorizationInfo& authzInfo,
        data::DataFilter filter);
        
private:
    static const int INDEX_BY_ACCOUNT_EMAIL = 1;
    static const int INDEX_BY_SYSTEM_ID = 2;

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
                api::SystemSharing, QnUuid, &api::SystemSharing::systemID>>
        >
    > AccountSystemAccessRoleDict;

    const AccountManager& m_accountManager;
    nx::db::DBManager* const m_dbManager;
    //!map<id, system>
    Cache<QnUuid, data::SystemData> m_cache;
    mutable QnMutex m_mutex;
    AccountSystemAccessRoleDict m_accountAccessRoleForSystem;
    ThreadSafeCounter m_startedAsyncCallsCounter;

    nx::db::DBResult insertSystemToDB(
        QSqlDatabase* const connection,
        const data::SystemRegistrationDataWithAccount& newSystem,
        data::SystemData* const systemData);
    void systemAdded(
        ThreadSafeCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccount systemRegistrationData,
        data::SystemData systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    nx::db::DBResult insertSystemSharingToDB(
        QSqlDatabase* const connection,
        const data::SystemSharing& systemSharing);
    void systemSharingAdded(
        ThreadSafeCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemSharing sytemSharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult deleteSystemFromDB(
        QSqlDatabase* const connection,
        const data::SystemID& systemID);
    void systemDeleted(
        ThreadSafeCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemID systemID,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult updateSharingInDB(
        QSqlDatabase* const connection,
        const data::SystemSharing& sharing);
    void sharingUpdated(
        ThreadSafeCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult dbResult,
        data::SystemSharing sharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchSystems(QSqlDatabase* connection, int* const /*dummy*/);
    nx::db::DBResult fetchSystemToAccountBinder(QSqlDatabase* connection, int* const /*dummy*/);
};

}   //cdb
}   //nx

#endif
