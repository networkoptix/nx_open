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

#include "access_control/types.h"
#include "data/system_data.h"
#include "db/db_manager.h"
#include "data_event.h"
#include "types.h"


namespace cdb {

typedef DataChangeEvent SystemChangeEvent;
typedef DataInsertUpdateEvent<SystemData> SystemInsertUpdateEvent;




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
    //!Binds system to account, associated with \a authInfo
    void bindSystemToAccount(
        const AuthenticationInfo& authInfo,
        const QnUuid& accountID,
        const QnUuid& systemID,
        const std::string& systemName,
        std::function<void(ResultCode, SystemData)> completionHandler );
    void unbindSystem(
        const AuthenticationInfo& authInfo,
        const QnUuid& systemID,
        std::function<void(ResultCode)> completionHandler );
    /*!
        \param eventReceiver Events related to returned data are reported to this functor
        \note It is garanteed that events are delivered after \a completionHandler has returned and only if request was a success
        \note If \a filter is empty, all systems authorized for \a authInfo are returned
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
     */
    void getSystems(
        const AuthenticationInfo& authInfo,
        const DataFilter& filter,
        std::function<void(ResultCode, TransactionSequence, std::vector<SystemData>)> completionHandler,
        std::function<void(DataChangeEvent)> eventReceiver = std::function<void(DataChangeEvent)>());
    void addSubscription(
        const AuthenticationInfo& authInfo,
        const QnUuid& systemID,
        const QnUuid& productID,
        std::function<void(ResultCode)> completionHandler );
    /*!
        \note if request can be completed immediately (e.g., data is present in internal cache) \a completionHandler will be invoked within this call
     */
    void getActiveSubscriptions(
        const AuthenticationInfo& authInfo,
        const QnUuid& systemID,
        std::function<void(ResultCode, std::vector<SubscriptionData>)> completionHandler );
    
    /*!
         Only notifications that match \a filter are returned
         \return receiver ID
     */
    int registerNotificationReceiver(
        const AuthenticationInfo& authInfo,
        const DataFilter& filter,
        TransactionSequence lastKnownTranSeq,
        std::function<void(const std::unique_ptr<SystemChangeEvent>&)> eventReceiver );
    void unregisterNotificationReceiver( int receiverID );

private:
    DBResult insertSystemToDB(
        DBTransaction& tran,
        const SystemData& newSystem,
        std::function<void(ResultCode, SystemData)> completionHandler );
};

}   //cdb

#endif
