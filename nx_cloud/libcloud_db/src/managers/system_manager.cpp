/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include "access_control/authorization_manager.h"
#include "data/cdb_ns.h"


namespace nx {
namespace cdb {

SystemManager::SystemManager( nx::db::DBManager* const dbManager )
:
    m_dbManager( dbManager )
{
}

void SystemManager::bindSystemToAccount(
    const AuthorizationInfo& authzInfo,
    data::SystemData&& systemData,
    std::function<void(ResultCode)> completionHandler )
{
    QnUuid accountID;
    if( authzInfo.get(cdb::param::accountID, &accountID) )
    {
        completionHandler( ResultCode::notAuthorized );
        return;
    }

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemData>(
        std::bind(&SystemManager::insertSystemToDB, this, _1, _2),
        std::move(systemData),
        std::bind(&SystemManager::systemAdded, this, _1, _2, std::move(completionHandler)) );
}

void SystemManager::getSystems(
    const AuthorizationInfo& authzInfo,
    const DataFilter& filter,
    std::function<void(ResultCode, TransactionSequence, std::vector<data::SystemData>)> completionHandler,
    std::function<void(DataChangeEvent)> eventReceiver )
{
    if( authzInfo.accessAllowedToOwnDataOnly() )
    {
        //adding account ID to query filter
        QnUuid accountID;
        //TODO authInfo or authzInfo? Get rid of this question at compile time
        if( !authzInfo.get(cdb::param::accountID, &accountID) )
        {
            completionHandler(
                ResultCode::notAuthorized,
                INVALID_TRANSACTION,
                std::vector<data::SystemData>() );
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

nx::db::DBResult SystemManager::insertSystemToDB(
    QSqlDatabase* const tran,
    const data::SystemData& newSystem )
{
    //sql: inserting (/updating?) record
    //sql: fetching newly created (/existing?) record (if needed)

    //TODO #ak sql: fetching transaction id
    TransactionSequence transactionID = 0;

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    nx::db::DBResult dbResult,
    data::SystemData systemData,
    std::function<void(ResultCode)> completionHandler )
{
    if( dbResult == nx::db::DBResult::ok )
    {
        //TODO #ak updating account cache
        //m_cache.recordModified( transactionID, newSystem.id, newSystem );
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
            ? ResultCode::ok
            : ResultCode::dbError );
}

}   //cdb
}   //nx
