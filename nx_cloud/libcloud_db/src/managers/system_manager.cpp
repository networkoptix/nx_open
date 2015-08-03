/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include "access_control/authorization_manager.h"
#include "data/cdb_ns.h"
#include "db/db_manager.h"


namespace cdb {

void SystemManager::bindSystemToAccount(
    const AuthenticationInfo& authInfo,
    const QnUuid& accountID,
    const QnUuid& systemID,
    const std::string& systemName,
    std::function<void(ResultCode, SystemData)> completionHandler )
{
    //TODO #ak authorization can be done before this method
    AuthorizationInfo authzInfo;
    if( !AuthorizationManager::instance()->authorize(
            authInfo,
            EntityType::system,
            DataActionType::insert,
            &authzInfo) )
    {
        completionHandler( ResultCode::notAuthorized, SystemData() );
        return;
    }
    
    if( authzInfo.accessAllowedToOwnDataOnly )
    {
        //comparing accountID and authenticated account id
        QnUuid authAccountID;
        if( !authInfo.params.get(cdb::param::accountID, &authAccountID) ||
            authAccountID != accountID)
        {
            completionHandler( ResultCode::notAuthorized, SystemData() );
            return;
        }
    }

    SystemData newSystem;
    //TODO filling newSystem

    DBManager::instance()->executeUpdate(
        std::bind(
            &SystemManager::insertSystemToDB,
            this, std::placeholders::_1,
            std::move(newSystem),
            std::move(completionHandler)) );
}

void SystemManager::getSystems(
    const AuthenticationInfo& authInfo,
    const DataFilter& filter,
    std::function<void(ResultCode, TransactionSequence, std::vector<SystemData>)> completionHandler,
    std::function<void(DataChangeEvent)> eventReceiver )
{
    //TODO #ak authorization can be done before this method
    //TODO authorization should be able to add new filter conditions
    AuthorizationInfo authzInfo;
    if( !AuthorizationManager::instance()->authorize(
            authInfo,
            EntityType::system,
            DataActionType::fetch,
            &authzInfo) )
    {
        completionHandler( ResultCode::notAuthorized, INVALID_TRANSACTION, std::vector<SystemData>() );
        return;
    }

    if( authzInfo.accessAllowedToOwnDataOnly )
    {
        //adding account ID to query filter
        QnUuid accountID;
        //TODO authInfo or authzInfo? Get rid of this question at compile time
        if( !authInfo.params.get(cdb::param::accountID, &accountID) )
        {
            completionHandler( ResultCode::notAuthorized, INVALID_TRANSACTION, std::vector<SystemData>() );
            return;
        }
    }

    //DataFilter combinedDataFilter( filter, authzInfo.dataFilter );

    //TODO selecting data using combinedDataFilter

    if( eventReceiver )
    {
        //TODO subscribing to data notifications
        //m_cache.
    }
}

DBResult SystemManager::insertSystemToDB(
    DBTransaction& tran,
    const SystemData& newSystem,
    std::function<void(ResultCode, SystemData)> completionHandler )
{
    //sql: inserting (/updating?) record
    //sql: fetching newly created (/existing?) record (if needed)

    //TODO #ak sql: fetching transaction id
    TransactionSequence transactionID = 0;

    const DBResult dbResult = tran.commit();
    if( dbResult == DBResult::ok )
    {
        //TODO #ak updating account cache
        //m_cache.recordModified( transactionID, newSystem.id, newSystem );
    }
    completionHandler(
        dbResult == DBResult::ok ? ResultCode::ok : ResultCode::dbError,
        newSystem );
    return dbResult;
}

}   //cdb
