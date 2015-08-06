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
    const AuthorizationInfo& authzInfo,
    const QnUuid& accountID,
    const QnUuid& systemID,
    const std::string& systemName,
    std::function<void(ResultCode, SystemData)> completionHandler )
{
    if( authzInfo.accessAllowedToOwnDataOnly() )
    {
        //comparing accountID and authenticated account id
        QnUuid authAccountID;
        if( !authzInfo.get(cdb::param::accountID, &authAccountID) ||
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
    const AuthorizationInfo& authzInfo,
    const DataFilter& filter,
    std::function<void(ResultCode, TransactionSequence, std::vector<SystemData>)> completionHandler,
    std::function<void(DataChangeEvent)> eventReceiver )
{
    if( authzInfo.accessAllowedToOwnDataOnly() )
    {
        //adding account ID to query filter
        QnUuid accountID;
        //TODO authInfo or authzInfo? Get rid of this question at compile time
        if( !authzInfo.get(cdb::param::accountID, &accountID) )
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
