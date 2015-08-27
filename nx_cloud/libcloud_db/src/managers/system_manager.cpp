/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#include "system_manager.h"

#include <QtSql/QSqlQuery>

#include <utils/common/log.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>

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
    data::SystemRegistrationData registrationData,
    std::function<void(ResultCode, data::SystemData)> completionHandler )
{
    QnUuid accountID;
    if( authzInfo.get(cdb::param::accountID, &accountID) )
    {
        completionHandler( ResultCode::notAuthorized, data::SystemData() );
        return;
    }

    data::SystemRegistrationDataWithAccountID registrationDataWithAccountID(
        std::move( registrationData ) );

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::SystemRegistrationDataWithAccountID, data::SystemData>(
        std::bind(&SystemManager::insertSystemToDB, this, _1, _2, _3),
        std::move(registrationDataWithAccountID),
        std::bind(&SystemManager::systemAdded, this, _1, _2, _3, std::move(completionHandler)) );
}

void SystemManager::getSystems(
    const AuthorizationInfo& authzInfo,
    const DataFilter& /*filter*/,
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
    QSqlDatabase* const connection,
    const data::SystemRegistrationDataWithAccountID& newSystem,
    data::SystemData* const systemData )
{
    QSqlQuery insertSystemQuery( *connection );
    insertSystemQuery.prepare(
        "INSERT INTO system( id, name, auth_key, account_id, status_code ) "
                   " VALUES( :id, :name, :authKey, :ownerAccountID, :status )");
    systemData->id = QnUuid::createUuid().toStdString();
    systemData->name = newSystem.name;
    systemData->authKey = QnUuid::createUuid().toStdString();
    systemData->ownerAccountID = newSystem.accountID.toStdString();
    systemData->status = data::SystemStatus::ssNotActivated;
    QnSql::bind( *systemData, &insertSystemQuery );
    if( !insertSystemQuery.exec() )
    {
        NX_LOG( lit( "Could not insert system into DB. %1" ).
            arg( connection->lastError().text() ), cl_logDEBUG1 );
        return db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

void SystemManager::systemAdded(
    nx::db::DBResult dbResult,
    data::SystemRegistrationDataWithAccountID systemRegistrationData,
    data::SystemData systemData,
    std::function<void(ResultCode, data::SystemData)> completionHandler )
{
    if( dbResult == nx::db::DBResult::ok )
    {
        //TODO #ak updating account cache
        //m_cache.recordModified( transactionID, newSystem.id, newSystem );
    }
    completionHandler(
        dbResult == nx::db::DBResult::ok
            ? ResultCode::ok
            : ResultCode::dbError,
        std::move(systemData) );
}

}   //cdb
}   //nx
