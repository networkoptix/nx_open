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
    const data::SystemData& systemData,
    std::function<void(ResultCode)> completionHandler )
{
    QnUuid accountID;
    if( authzInfo.get(cdb::param::accountID, &accountID) )
    {
        completionHandler( ResultCode::notAuthorized );
        return;
    }

    using namespace std::placeholders;
    db::DBManager::instance()->executeUpdate(
        std::bind(&SystemManager::insertSystemToDB, this, _1, _2),
        std::move(systemData),
        std::bind(&SystemManager::systemAdded, this, _1, _2, std::move(completionHandler)) );
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

db::DBResult SystemManager::insertSystemToDB(
    db::DBTransaction& tran,
    const SystemData& newSystem )
{
    //sql: inserting (/updating?) record
    //sql: fetching newly created (/existing?) record (if needed)

    //TODO #ak sql: fetching transaction id
    TransactionSequence transactionID = 0;

    return db::DBResult::ok;
}

void SystemManager::systemAdded(
    db::DBResult resultCode,
    SystemData&& systemData,
    std::function<void(ResultCode)> completionHandler )
{
    if( dbResult == db::DBResult::ok )
    {
        //TODO #ak updating account cache
        //m_cache.recordModified( transactionID, newSystem.id, newSystem );
    }
    completionHandler( dbResult == db::DBResult::ok ? ResultCode::ok : ResultCode::dbError );
}

}   //cdb















