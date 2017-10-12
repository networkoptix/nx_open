#include "detail_db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

#include "../async_sql_query_executor.h"

namespace nx {
namespace utils {
namespace db {
namespace detail {

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

} // namespace

//-------------------------------------------------------------------------------------------------

DbStructureUpdater::DbStructureUpdater(
    const std::string& schemaName,
    AbstractAsyncSqlQueryExecutor* const queryExecutor)
    :
    m_schemaName(schemaName),
    m_queryExecutor(queryExecutor),
    m_initialVersion(0)
{
    m_updateScripts.emplace_back(QByteArray()); //< Just a placeholder to keep version number.
}

void DbStructureUpdater::setInitialVersion(unsigned int version)
{
    m_initialVersion = version;
}

void DbStructureUpdater::addUpdateScript(QByteArray updateScript)
{
    m_updateScripts.emplace_back(std::move(updateScript));
}

void DbStructureUpdater::addUpdateScript(
    std::map<RdbmsDriverType, QByteArray> scriptByDbType)
{
    m_updateScripts.emplace_back(std::move(scriptByDbType));
}

void DbStructureUpdater::addUpdateFunc(UpdateFunc dbUpdateFunc)
{
    m_updateScripts.emplace_back(std::move(dbUpdateFunc));
}

void DbStructureUpdater::addFullSchemaScript(
    unsigned int version,
    QByteArray createSchemaScript)
{
    NX_ASSERT(version > 0);
    m_fullSchemaScriptByVersion.emplace(version, std::move(createSchemaScript));
}

unsigned int DbStructureUpdater::maxKnownVersion() const
{
    return m_initialVersion + (unsigned int) m_updateScripts.size();
}

void DbStructureUpdater::setVersionToUpdateTo(unsigned int version)
{
    m_versionToUpdateTo = version;
}

void DbStructureUpdater::updateStruct(QueryContext* const queryContext)
{
    DbSchemaState dbState = analyzeDbSchemaState(queryContext);
    if (dbState.version < m_initialVersion)
    {
        NX_LOGX(lit("DB of version %1 cannot be upgraded! Minimal supported version is %2")
            .arg(dbState.version).arg(m_initialVersion), cl_logERROR);
        throw Exception(DBResult::notFound);
    }

    if (!dbState.someSchemaExists)
    {
        const auto dbResult = createInitialSchema(queryContext, &dbState);
        if (dbResult != DBResult::ok)
            throw Exception(dbResult);
    }

    auto dbResult = applyScriptsMissingInCurrentDb(queryContext, &dbState);
    if (dbResult != DBResult::ok)
        throw Exception(dbResult);

    dbResult = updateDbVersion(queryContext, dbState);
    if (dbResult != DBResult::ok)
        throw Exception(dbResult);
}

DbStructureUpdater::DbSchemaState DbStructureUpdater::analyzeDbSchemaState(
    nx::utils::db::QueryContext* const queryContext)
{
    DbStructureUpdater::DbSchemaState dbSchemaState{ m_initialVersion, false };

    //reading current DB version
    QSqlQuery fetchDbVersionQuery(*queryContext->connection());
    fetchDbVersionQuery.prepare(
        "SELECT db_version FROM db_version_data WHERE schema_name=:schemaName");
    fetchDbVersionQuery.bindValue(":schemaName", QString::fromStdString(m_schemaName));
    if (fetchDbVersionQuery.exec() && fetchDbVersionQuery.next())
    {
        dbSchemaState.version = fetchDbVersionQuery.value(lit("db_version")).toUInt();
        dbSchemaState.someSchemaExists = true;
    }

    return dbSchemaState;
}

DBResult DbStructureUpdater::createInitialSchema(
    nx::utils::db::QueryContext* const queryContext,
    DbSchemaState* dbSchemaState)
{
    dbSchemaState->version = 1;

    if (!m_fullSchemaScriptByVersion.empty())
    {
        // Applying full schema.
        if (!execSqlScript(
                queryContext,
                m_fullSchemaScriptByVersion.rbegin()->second,
                RdbmsDriverType::unknown))
        {
            NX_LOG(lit("DbStructureUpdater. Failed to create schema of version %1: %2")
                .arg(m_fullSchemaScriptByVersion.rbegin()->first)
                .arg(queryContext->connection()->lastError().text()), cl_logWARNING);
            return DBResult::ioError;
        }
        dbSchemaState->version = m_fullSchemaScriptByVersion.rbegin()->first;
    }

    dbSchemaState->someSchemaExists = true;

    return DBResult::ok;
}

DBResult DbStructureUpdater::applyScriptsMissingInCurrentDb(
    nx::utils::db::QueryContext* queryContext,
    DbSchemaState* dbState)
{
    while (gotScriptForUpdate(dbState))
    {
        const auto dbResult = applyNextUpdateScript(queryContext, dbState);
        if (dbResult != DBResult::ok)
            return dbResult;
    }

    return DBResult::ok;
}

bool DbStructureUpdater::gotScriptForUpdate(DbSchemaState* dbState) const
{
    auto versionToUpdateTo = m_initialVersion + m_updateScripts.size();
    if (m_versionToUpdateTo)
        versionToUpdateTo = *m_versionToUpdateTo;

    return static_cast<size_t>(dbState->version) < versionToUpdateTo;
}

DBResult DbStructureUpdater::applyNextUpdateScript(
    nx::utils::db::QueryContext* queryContext,
    DbSchemaState* dbState)
{
    NX_LOGX(lm("Updating structure to version %1").arg(dbState->version), cl_logDEBUG2);

    if (!execDbUpdate(
            m_updateScripts[dbState->version - m_initialVersion],
            queryContext))
    {
        NX_LOG(lit("DbStructureUpdater. Failure updating to version %1: %2")
            .arg(dbState->version).arg(queryContext->connection()->lastError().text()),
            cl_logWARNING);
        return DBResult::ioError;
    }

    ++dbState->version;

    return DBResult::ok;
}

DBResult DbStructureUpdater::updateDbVersion(
    nx::utils::db::QueryContext* const queryContext,
    const DbSchemaState& dbSchemaState)
{
    QSqlQuery updateDbVersionQuery(*queryContext->connection());
    updateDbVersionQuery.prepare(R"sql(
        REPLACE INTO db_version_data(schema_name, db_version)
        VALUES (:schemaName, :dbVersion)
    )sql");
    updateDbVersionQuery.bindValue(":schemaName", QString::fromStdString(m_schemaName));
    updateDbVersionQuery.bindValue(":dbVersion", dbSchemaState.version);
    return updateDbVersionQuery.exec() ? DBResult::ok : DBResult::ioError;
}

bool DbStructureUpdater::execDbUpdate(
    const DbUpdate& dbUpdate,
    nx::utils::db::QueryContext* const queryContext)
{
    if (!dbUpdate.dbTypeToSqlScript.empty())
    {
        if (!execStructureUpdateTask(dbUpdate.dbTypeToSqlScript, queryContext))
            return false;
    }

    if (dbUpdate.func)
    {
        if (dbUpdate.func(queryContext) != nx::utils::db::DBResult::ok)
        {
            NX_LOGX(lm("Error executing update function"), cl_logWARNING);
            return false;
        }
    }

    return true;
}

bool DbStructureUpdater::execStructureUpdateTask(
    const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
    nx::utils::db::QueryContext* const queryContext)
{
    const auto driverType = m_queryExecutor->connectionOptions().driverType;

    std::map<RdbmsDriverType, QByteArray>::const_iterator selectedScriptIter =
        selectSuitableScript(dbTypeToScript, driverType);
    if (selectedScriptIter == dbTypeToScript.end())
    {
        NX_LOGX(lm("Could not find script version for DB %1. Aborting...")
            .arg(driverType), cl_logDEBUG1);
        return false;
    }

    return execSqlScript(
        queryContext,
        selectedScriptIter->second,
        selectedScriptIter->first);
}

std::map<RdbmsDriverType, QByteArray>::const_iterator DbStructureUpdater::selectSuitableScript(
    const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
    RdbmsDriverType driverType) const
{
    auto properScriptIter = dbTypeToScript.find(driverType);
    if (properScriptIter == dbTypeToScript.end())
        properScriptIter = dbTypeToScript.find(RdbmsDriverType::unknown);
    if (properScriptIter == dbTypeToScript.end())
        return dbTypeToScript.end();

    return properScriptIter;
}

bool DbStructureUpdater::execSqlScript(
    nx::utils::db::QueryContext* const queryContext,
    QByteArray sqlScript,
    RdbmsDriverType sqlScriptDialect)
{
    QByteArray scriptText;
    if (sqlScriptDialect == RdbmsDriverType::unknown)
        scriptText = fixSqlDialect(sqlScript, m_queryExecutor->connectionOptions().driverType);
    else
        scriptText = sqlScript;

    return m_queryExecutor->execSqlScriptSync(scriptText, queryContext) == DBResult::ok;
}

QByteArray DbStructureUpdater::fixSqlDialect(
    QByteArray initialScript, RdbmsDriverType targetDialect)
{
    QByteArray script = initialScript;
    for (const auto& replacementCtx: kSqlReplacements)
    {
        const auto it = replacementCtx.replacementsByDriverName.find(targetDialect);
        if (it != replacementCtx.replacementsByDriverName.end())
            script.replace(replacementCtx.key, it->second);
        else
            script.replace(replacementCtx.key, replacementCtx.defaultValue);
    }
    return script;
}

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
