/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_system_manager_h
#define cloud_db_system_manager_h

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <utils/db/db_manager.h>
#include <utils/thread/mutex.h>

#include "access_control/auth_types.h"
#include "cache.h"
#include "data/system_data.h"
#include "data_view.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

/*!
    Provides methods for manipulating system data on persisent storage.
    Calls \a DBManager instance to perform DB manipulation. SQL requests are written in this class
    \note All data can be cached
*/
class SystemManager
{
public:
    /*!
        Fills internal cache
        \throw std::runtime_error In case of failure to pre-fill data cache
    */
    SystemManager(nx::db::DBManager* const dbManager) throw(std::runtime_error);

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
        DataFilter filter,
        std::function<void(api::ResultCode, data::SystemDataList)> completionHandler );
    //!Share system with specified account. Operation allowed for system owner and editor_with_sharing only
    void shareSystem(
        const AuthorizationInfo& authzInfo,
        data::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler);
    //!Provides list of cloud accounts who have access to this system
    void getCloudUsersOfSystem(
        const AuthorizationInfo& authzInfo,
        const data::SystemID& systemID,
        std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler);

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
        - \a accountID has no rights for \a systemID
        - \a accountID or \a systemID is unknown
    */
    api::SystemAccessRole getAccountRightsForSystem(
        const QnUuid& accountID, const QnUuid& systemID) const;

    //!Create data view restricted by \a authzInfo and \a filter
    DataView<data::SystemData> createView(
        const AuthorizationInfo& authzInfo,
        DataFilter filter);
        
private:
    nx::db::DBManager* const m_dbManager;
    //!map<id, system>
    Cache<QnUuid, data::SystemData> m_cache;
    mutable QnMutex m_mutex;
    //!map<pair<accountID, systemID>, accessRole>
    std::map<std::pair<QnUuid, QnUuid>, api::SystemAccessRole> m_accountAccessRoleForSystem;

    nx::db::DBResult insertSystemToDB(
        QSqlDatabase* const connection,
        const data::SystemRegistrationDataWithAccountID& newSystem,
        data::SystemData* const systemData);
    void systemAdded(
        nx::db::DBResult dbResult,
        data::SystemRegistrationDataWithAccountID systemRegistrationData,
        data::SystemData systemData,
        std::function<void(api::ResultCode, data::SystemData)> completionHandler);

    nx::db::DBResult insertSystemSharingToDB(
        QSqlDatabase* const connection,
        const data::SystemSharing& systemSharing);
    void systemSharingAdded(
        nx::db::DBResult dbResult,
        data::SystemSharing sytemSharing,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult deleteSystemFromDB(
        QSqlDatabase* const connection,
        const data::SystemID& systemID);
    void systemDeleted(
        nx::db::DBResult dbResult,
        data::SystemID systemID,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchSystems(QSqlDatabase* connection, int* const /*dummy*/);
    nx::db::DBResult fetchSystemToAccountBinder(QSqlDatabase* connection, int* const /*dummy*/);
};

}   //cdb
}   //nx

#endif
