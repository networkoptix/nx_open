#include "db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

#include <utils/db/db_helper.h>

#include "async_sql_query_executor.h"

namespace nx {
namespace db {

namespace {

struct SqlReplacementContext
{
    QByteArray key;
    QByteArray defaultValue;
    /** map<Sql driver, replacement>. */
    std::map<RdbmsDriverType, QByteArray> replacementsByDriverName;
};

typedef std::vector<SqlReplacementContext> ReplacementsDictionary;

static ReplacementsDictionary initializeReplacements()
{
    ReplacementsDictionary replacements;

    {
        SqlReplacementContext replacement;
        replacement.key = "%bigint_primary_key_auto_increment%";
        replacement.replacementsByDriverName.emplace(
            RdbmsDriverType::sqlite, "INTEGER PRIMARY KEY AUTOINCREMENT");
        replacement.defaultValue = "BIGINT PRIMARY KEY AUTO_INCREMENT"; //< As in Mysql.
        replacements.push_back(std::move(replacement));
    }

    return replacements;
}

static const ReplacementsDictionary kSqlReplacements = initializeReplacements();

static const char kCreateDbVersionTable[] = 
R"sql(

CREATE TABLE db_version_data (
    db_version      integer NOT NULL DEFAULT 0
);

INSERT INTO db_version_data (db_version) VALUES (0);

)sql";

} // namespace


DBStructureUpdater::DBStructureUpdater(AbstractAsyncSqlQueryExecutor* const queryExecutor):
    m_queryExecutor(queryExecutor),
    m_initialVersion(0)
{
    m_updateScripts.emplace_back(QByteArray(kCreateDbVersionTable));
}

void DBStructureUpdater::setInitialVersion(unsigned int version)
{
    m_initialVersion = version;
}

void DBStructureUpdater::addUpdateScript(QByteArray updateScript)
{
    m_updateScripts.emplace_back(std::move(updateScript));
}

void DBStructureUpdater::addUpdateFunc(DbUpdateFunc dbUpdateFunc)
{
    m_updateScripts.emplace_back(std::move(dbUpdateFunc));
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
    m_queryExecutor->executeUpdate(
        std::bind(&DBStructureUpdater::updateDbInternal, this, std::placeholders::_1),
        [this](nx::db::QueryContext* /*connection*/, DBResult dbResult)
        {
            m_dbUpdatePromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get() == DBResult::ok;
}

DBResult DBStructureUpdater::updateDbInternal(nx::db::QueryContext* const queryContext)
{
    //reading current DB version
    QSqlQuery fetchDbVersionQuery(*queryContext->connection());
    fetchDbVersionQuery.prepare(lit("SELECT db_version FROM db_version_data"));
    qint64 dbVersion = m_initialVersion;
    //absense of table db_version_data is normal: DB is just empty
    bool someSchemaExists = false;
    if (fetchDbVersionQuery.exec() && fetchDbVersionQuery.next())
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
        if (!execSQLScript(kCreateDbVersionTable, queryContext))
        {
            NX_LOG(lit("DBStructureUpdater. Failed to apply kCreateDbVersionTable script. %1")
                .arg(queryContext->connection()->lastError().text()), cl_logWARNING);
            return DBResult::ioError;
        }
        dbVersion = 1;

        if (!m_fullSchemaScriptByVersion.empty())
        {
            //applying full schema
            if (!execSQLScript(m_fullSchemaScriptByVersion.rbegin()->second, queryContext))
            {
                NX_LOG(lit("DBStructureUpdater. Failed to create schema of version %1: %2").
                    arg(m_fullSchemaScriptByVersion.rbegin()->first)
                    .arg(queryContext->connection()->lastError().text()), cl_logWARNING);
                return DBResult::ioError;
            }
            dbVersion = m_fullSchemaScriptByVersion.rbegin()->first;
        }
    }

    // Applying scripts missing in current DB.
    for (;
        static_cast< size_t >(dbVersion) < (m_initialVersion + m_updateScripts.size());
        ++dbVersion)
    {
        NX_LOGX(lm("Updating structure to version %1. %2")
            .arg(dbVersion).arg(m_updateScripts[dbVersion - m_initialVersion].sqlScript),
            cl_logDEBUG2);

        if (!execDbUpdate(m_updateScripts[dbVersion - m_initialVersion], queryContext))
        {
            NX_LOG(lit("DBStructureUpdater. Failure updating to version %1: %2").
                arg(dbVersion).arg(queryContext->connection()->lastError().text()),
                cl_logWARNING);
            return DBResult::ioError;
        }
    }

    //updating db version
    QSqlQuery updateDbVersion(*queryContext->connection());
    updateDbVersion.prepare(lit("UPDATE db_version_data SET db_version = :dbVersion"));
    updateDbVersion.bindValue(lit(":dbVersion"), dbVersion);
    return updateDbVersion.exec() ? DBResult::ok : DBResult::ioError;
}

bool DBStructureUpdater::execDbUpdate(
    const DbUpdate& dbUpdate,
    nx::db::QueryContext* const queryContext)
{
    if (!dbUpdate.sqlScript.isEmpty())
    {
        if (!execSQLScript(dbUpdate.sqlScript, queryContext))
            return false;
    }

    if (dbUpdate.func)
    {
        if (dbUpdate.func(queryContext) != nx::db::DBResult::ok)
        {
            NX_LOGX(lm("Error executing update function"), cl_logWARNING);
            return false;
        }
    }

    return true;
}

bool DBStructureUpdater::execSQLScript(
    QByteArray script,
    nx::db::QueryContext* const queryContext)
{
    const auto driverType = m_queryExecutor->connectionOptions().driverType;

    // Applying replacements.
    for (const auto& replacementCtx: kSqlReplacements)
    {
        const auto it = replacementCtx.replacementsByDriverName.find(driverType);
        if (it != replacementCtx.replacementsByDriverName.end())
            script.replace(replacementCtx.key, it->second);
        else
            script.replace(replacementCtx.key, replacementCtx.defaultValue);
    }

    return m_queryExecutor->execSqlScriptSync(script, queryContext) == DBResult::ok;
}

} // namespace db
} // namespace nx
