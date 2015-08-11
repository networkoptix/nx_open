/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#include "db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <utils/common/log.h>

#include "db_manager.h"
#include "structure_update_statements.h"


namespace nx {
namespace cdb {
namespace db {


DBStructureUpdater::DBStructureUpdater( DBManager* const dbManager )
:
    m_dbManager( dbManager )
{
    m_updateScripts.push_back( createDbVersionTables );
}

bool DBStructureUpdater::updateStructSync()
{
    m_dbUpdatePromise = std::promise<DBResult>();
    auto future = m_dbUpdatePromise.get_future();

    //starting async operation
    m_dbManager->executeUpdate(
        std::bind( &DBStructureUpdater::updateDbInternal, this, std::placeholders::_1 ),
        [this]( DBResult dbResult ) {
            m_dbUpdatePromise.set_value( dbResult );
        } );

    //waiting for completion
    future.wait();
    return future.get() == DBResult::ok;
}

DBResult DBStructureUpdater::updateDbInternal( QSqlDatabase* const dbConnection )
{
    //reading current DB version
    QSqlQuery fetchDbVersionQuery( *dbConnection );
    fetchDbVersionQuery.prepare( "SELECT db_version FROM internal_data" );
    size_t dbVersion = 0;
    //absense of table internal_data is normal: DB is just empty
    if( fetchDbVersionQuery.exec() && fetchDbVersionQuery.next() )
        dbVersion = fetchDbVersionQuery.value("db_version").toUInt();

    //applying scripts missing in current DB
    for( ;
        dbVersion < m_updateScripts.size();
        ++dbVersion )
    {
        QSqlQuery updateQuery( *dbConnection );
        updateQuery.prepare( m_updateScripts[dbVersion] );
        if( !updateQuery.exec() )
        {
            NX_LOG( lit("DBStructureUpdater. Failure updating to version %1: %2").
                arg( dbVersion ).arg( dbConnection->lastError().text() ), cl_logWARNING );
            return DBResult::ioError;
        }
    }

    //updating db version
    QSqlQuery updateDbVersion( *dbConnection );
    updateDbVersion.prepare( "UPDATE internal_data SET db_version = :dbVersion" );
    updateDbVersion.bindValue( ":dbVersion", dbVersion );
    return updateDbVersion.exec() ? DBResult::ok : DBResult::ioError;
}

}   //db
}   //cdb
}   //nx
