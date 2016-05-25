/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#include "db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <utils/db/db_helper.h>

#include "async_sql_query_executor.h"


namespace nx {
namespace db {

static const char kCreateDbVersionTable[] = 
"                                                   \
CREATE TABLE db_version_data (                      \
    db_version      integer NOT NULL DEFAULT 0      \
);                                                  \
\n                                                  \
INSERT INTO db_version_data ( db_version )          \
                   VALUES ( 0 );                    \
";



DBStructureUpdater::DBStructureUpdater(AsyncSqlQueryExecutor* const dbManager)
:
    m_dbManager(dbManager),
    m_initialVersion(0)
{
    m_updateScripts.push_back(kCreateDbVersionTable);
}

void DBStructureUpdater::setInitialVersion(unsigned int version)
{
    m_initialVersion = version;
}

void DBStructureUpdater::addUpdateScript(QByteArray updateScript)
{
    m_updateScripts.push_back(std::move(updateScript));
}

void DBStructureUpdater::addFullSchemaScript(
    unsigned int version,
    QByteArray createSchemaScript)
{
    NX_ASSERT(version > 0);
    m_fullSchemaScriptByVersion.emplace(version, std::move(createSchemaScript));
}

bool DBStructureUpdater::updateStructSync()
{
    m_dbUpdatePromise = nx::utils::promise<DBResult>();
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
    qint64 dbVersion = m_initialVersion;
    //absense of table db_version_data is normal: DB is just empty
    bool someSchemaExists = false;
    if( fetchDbVersionQuery.exec() && fetchDbVersionQuery.next() )
    {
        dbVersion = fetchDbVersionQuery.value(lit("db_version")).toUInt();
        someSchemaExists = true;
    }
    fetchDbVersionQuery.finish();

    if (dbVersion < m_initialVersion)
    {
        NX_LOGX(lit("DB of version %1 cannot be upgraded! Minimal supported version is %2")
            .arg(dbVersion).arg(m_initialVersion), cl_logERROR);
        return DBResult::notFound;
    }

    if (!someSchemaExists)
    {
        if (!QnDbHelper::execSQLScript(kCreateDbVersionTable, *dbConnection))
        {
            NX_LOG(lit("DBStructureUpdater. Failed to apply kCreateDbVersionTable script. %1")
                .arg(dbConnection->lastError().text()), cl_logWARNING);
            return DBResult::ioError;
        }
        dbVersion = 1;

        if (!m_fullSchemaScriptByVersion.empty())
        {
            //applying full schema
            if (!QnDbHelper::execSQLScript(m_fullSchemaScriptByVersion.rbegin()->second, *dbConnection))
            {
                NX_LOG(lit("DBStructureUpdater. Failed to create schema of version %1: %2").
                    arg(m_fullSchemaScriptByVersion.rbegin()->first)
                    .arg(dbConnection->lastError().text()), cl_logWARNING);
                return DBResult::ioError;
            }
            dbVersion = m_fullSchemaScriptByVersion.rbegin()->first;
        }
    }

    //applying scripts missing in current DB
    for( ;
        static_cast< size_t >( dbVersion ) < (m_initialVersion+m_updateScripts.size());
        ++dbVersion )
    {
        if( !QnDbHelper::execSQLScript( m_updateScripts[dbVersion-m_initialVersion], *dbConnection ) )
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
