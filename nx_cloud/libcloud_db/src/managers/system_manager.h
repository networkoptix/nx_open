/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_system_manager_h
#define cloud_db_system_manager_h

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <utils/db/db_manager.h>

#include "access_control/auth_types.h"
#include "data/system_data.h"
#include "data_event.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

typedef DataChangeEvent SystemChangeEvent;
typedef DataInsertUpdateEvent<data::SystemData> SystemInsertUpdateEvent;




/*
 
 NOTE (any fetch data request of any manager)
 
 If request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked immediately within fetch data function.
 Actually, in current implementation this is ALWAYS so.
 TODO If it always so, maybe async call is not needed?
 
 */





/*!
    Provides methods for manipulating system data on persisent storage.
    Calls \a DBManager instance to perform DB manipulation. SQL requests are written in this class
    \note All data can be cached
 */
class SystemManager
{
public:
    SystemManager( nx::db::DBManager* const dbManager );

    //!Binds system to an account associated with \a authzInfo
    void bindSystemToAccount(
        const AuthorizationInfo& authzInfo,
        data::SystemData&& systemData,
        std::function<void(ResultCode)> completionHandler );
    void unbindSystem(
        const AuthorizationInfo& authzInfo,
        const QnUuid& systemID,
        std::function<void(ResultCode)> completionHandler );
    /*!
        \param eventReceiver Events related to returned data are reported to this functor
        \note It is garanteed that events are delivered after \a completionHandler has returned and only if request was a success
        \note If \a filter is empty, all systems authorized for \a authInfo are returned
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
     */
    void getSystems(
        const AuthorizationInfo& authzInfo,
        const DataFilter& filter,
        std::function<void(ResultCode, TransactionSequence, std::vector<data::SystemData>)> completionHandler,
        std::function<void(DataChangeEvent)> eventReceiver = std::function<void(DataChangeEvent)>());
    void addSubscription(
        const AuthorizationInfo& authzInfo,
        const QnUuid& systemID,
        const QnUuid& productID,
        std::function<void(ResultCode)> completionHandler );
    /*!
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
     */
    void getActiveSubscriptions(
        const AuthorizationInfo& authzInfo,
        const QnUuid& systemID,
        std::function<void(ResultCode, std::vector<data::SubscriptionData>)> completionHandler );
    
    /*!
         Only notifications that match \a filter are returned
         \return receiver ID
     */
    int registerNotificationReceiver(
        const AuthorizationInfo& authzInfo,
        const DataFilter& filter,
        TransactionSequence lastKnownTranSeq,
        std::function<void(const std::unique_ptr<SystemChangeEvent>&)> eventReceiver );
    void unregisterNotificationReceiver( int receiverID );

private:
    nx::db::DBManager* const m_dbManager;

    nx::db::DBResult insertSystemToDB(
        QSqlDatabase* const tran,
        const data::SystemData& newSystem );
    void systemAdded(
        nx::db::DBResult resultCode,
        data::SystemData systemData,
        std::function<void(ResultCode)> completionHandler );
};

}   //cdb
}   //nx

#endif
