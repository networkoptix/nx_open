/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#include "db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <utils/common/log.h>
#include <utils/db/db_helper.h>

#include "db_manager.h"


namespace nx {
namespace db {

static const char createDbVersionTable[] = 
"                                                   \
CREATE TABLE db_version_data (                      \
    db_version      integer NOT NULL DEFAULT 0      \
);                                                  \
\n                                                  \
INSERT INTO db_version_data ( db_version )          \
                   VALUES ( 0 );                    \
";



DBStructureUpdater::DBStructureUpdater( DBManager* const dbManager )
:
    m_dbManager( dbManager )
{
    m_updateScripts.push_back( createDbVersionTable );
}

void DBStructureUpdater::addUpdateScript( const QByteArray& updateScript )
{
    m_updateScripts.push_back( updateScript );
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
    fetchDbVersionQuery.prepare( lit("SELECT db_version FROM db_version_data") );
    qint64 dbVersion = 0;
    //absense of table db_version_data is normal: DB is just empty
    if( fetchDbVersionQuery.exec() && fetchDbVersionQuery.next() )
        dbVersion = fetchDbVersionQuery.value(lit("db_version")).toUInt();

    //applying scripts missing in current DB
    for( ;
        static_cast< size_t >( dbVersion ) < m_updateScripts.size();
        ++dbVersion )
    {
        if( !QnDbHelper::execSQLScript( m_updateScripts[dbVersion], *dbConnection ) )
        {
            NX_LOG( lit("DBStructureUpdater. Failure updating to version %1: %2").
                arg( dbVersion ).arg( dbConnection->lastError().text() ), cl_logWARNING );
            return DBResult::ioError;
        }
    }

    //updating db version
    QSqlQuery updateDbVersion( *dbConnection );
    updateDbVersion.prepare( lit("UPDATE db_version_data SET db_version = :dbVersion") );
    updateDbVersion.bindValue( lit(":dbVersion"), dbVersion );
    return updateDbVersion.exec() ? DBResult::ok : DBResult::ioError;
}

}   //db
}   //nx
