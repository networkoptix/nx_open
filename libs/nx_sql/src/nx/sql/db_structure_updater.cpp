// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_structure_updater.h"

#include <nx/utils/log/log.h>

#include "async_sql_query_executor.h"
#include "sql_query_execution_helper.h"
#include "query.h"
#include "types.h"

namespace nx::sql {

static constexpr char kMakeSchemaNameColumnLonger[] = R"sql(

ALTER TABLE db_schema_applied_scripts
RENAME TO db_schema_applied_scripts_old;

CREATE TABLE db_schema_applied_scripts(
    schema_name VARCHAR(128),
    script_name VARCHAR(64),
    CONSTRAINT UC_script_128 UNIQUE (schema_name, script_name)
);

INSERT INTO db_schema_applied_scripts
    SELECT schema_name, script_name
    FROM db_schema_applied_scripts_old;

DROP TABLE db_schema_applied_scripts_old;

)sql";

// CB-2010.
static constexpr char kAddMissingPrimaryKeys_mysql[] = R"sql(

ALTER TABLE db_schema_applied_scripts ADD PRIMARY KEY (schema_name, script_name);

)sql";

// CB-2010.
static constexpr char kAddMissingPrimaryKeys_sqlite[] = R"sql(

ALTER TABLE db_schema_applied_scripts RENAME TO db_schema_applied_scripts_bak;

CREATE TABLE db_schema_applied_scripts(
    schema_name VARCHAR(128),
    script_name VARCHAR(64),
    PRIMARY KEY(schema_name, script_name)
);

INSERT INTO db_schema_applied_scripts
SELECT * FROM db_schema_applied_scripts_bak;

DROP TABLE db_schema_applied_scripts_bak;

)sql";

//-------------------------------------------------------------------------------------------------

DbStructureUpdater::DbStructureUpdater(
    const std::string& schemaName,
    AbstractAsyncSqlQueryExecutor* const queryExecutor)
    :
    DbStructureUpdater(schemaName)
{
    m_queryExecutor = queryExecutor;
}

DbStructureUpdater::DbStructureUpdater(const std::string& schemaName):
    m_schemaUpdater(schemaName),
    m_schemaName(schemaName)
{
}

bool DbStructureUpdater::addUpdateScript(std::string name, std::string updateScript)
{
    return m_schemaUpdater.addUpdateScript(name, std::move(updateScript));
}

bool DbStructureUpdater::addUpdateScript(
    std::string name,
    std::map<RdbmsDriverType, std::string> scriptByDbType)
{
    return m_schemaUpdater.addUpdateScript(name, std::move(scriptByDbType));
}

bool DbStructureUpdater::addUpdateFunc(std::string name, UpdateFunc dbUpdateFunc)
{
    return m_schemaUpdater.addUpdateFunc(name, std::move(dbUpdateFunc));
}

void DbStructureUpdater::updateStructSyncThrow()
{
    try
    {
        m_queryExecutor->executeUpdateSync(
            [this](auto&&... args) { return updateStruct(std::forward<decltype(args)>(args)...); });
    }
    catch (const sql::Exception& e)
    {
        NX_ERROR(this, "Error updating db schema \"%1\". %2", m_schemaName, e.what());
        throw;
    }
}

bool DbStructureUpdater::updateStructSync()
{
    try
    {
        updateStructSyncThrow();
        return true;
    }
    catch (const sql::Exception&)
    {
        return false;
    }
}

void DbStructureUpdater::updateStruct(nx::sql::QueryContext* queryContext)
{
    // Checking maintenance data.
    if (!appliedScriptsTableExists(queryContext))
        createAppliedScriptsTable(queryContext);

    updateMaintenanceDbScheme(queryContext);

    // Migrating applied scripts info from db_version table into new applied_scripts.
    if (dbVersionTableExists(queryContext))
        syncDbVersionAndAppliedScriptsTables(queryContext);

    m_schemaUpdater.updateStruct(queryContext);
}

bool DbStructureUpdater::exists() const
{
    try
    {
        return m_queryExecutor->executeUpdateSync(
            [this](nx::sql::QueryContext* queryContext)
            {
                return schemaExists(queryContext, m_schemaName);
            });
    }
    catch (const std::exception&)
    {
        // Considering connection failure as an non-existent DB.
        return false;
    }
}

bool DbStructureUpdater::schemaExists(
    nx::sql::QueryContext* queryContext,
    const std::string& name)
{
    try
    {
        auto query = queryContext->connection()->createQuery();
        query->prepare(R"sql(
            SELECT script_name FROM db_schema_applied_scripts
            WHERE schema_name = ?
        )sql");

        query->addBindValue(name);
        query->exec();

        return query->next();
    }
    catch (const std::exception&)
    {
        // Considering connection failure as an non-existent DB.
        return false;
    }
}

void DbStructureUpdater::renameSchema(
    nx::sql::QueryContext* queryContext,
    const std::string& newName,
    const std::string& oldName)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        UPDATE db_schema_applied_scripts SET schema_name = ?
        WHERE schema_name = ?
    )sql");

    query->addBindValue(newName);
    query->addBindValue(oldName);
    query->exec();
}

bool DbStructureUpdater::appliedScriptsTableExists(QueryContext* queryContext)
{
    return queryContext->connection()->tableExist("db_schema_applied_scripts");
}

void DbStructureUpdater::createAppliedScriptsTable(QueryContext* queryContext)
{
    NX_DEBUG(this, "Creating db_schema_applied_scripts table");

    SqlQueryExecutionHelper::execSQLScript(
        queryContext,
        R"sql(
            CREATE TABLE db_schema_applied_scripts(
                schema_name VARCHAR(128),
                script_name VARCHAR(64),
                PRIMARY KEY(schema_name, script_name)
            );

            INSERT INTO db_schema_applied_scripts(schema_name, script_name)
                VALUES("maintenance_C5DB4205-00E8-4BB0-8DEC-E982C7C9AFEF", "kMakeSchemaNameColumnLonger");

            INSERT INTO db_schema_applied_scripts(schema_name, script_name)
                VALUES("maintenance_C5DB4205-00E8-4BB0-8DEC-E982C7C9AFEF", "kAddMissingPrimaryKeys");
        )sql");
}

void DbStructureUpdater::updateMaintenanceDbScheme(QueryContext* queryContext)
{
    detail::DbStructureUpdater schemaUpdater(
        "maintenance_C5DB4205-00E8-4BB0-8DEC-E982C7C9AFEF");

    schemaUpdater.addUpdateScript("kMakeSchemaNameColumnLonger", kMakeSchemaNameColumnLonger);
    schemaUpdater.addUpdateScript("kAddMissingPrimaryKeys", {
        {nx::sql::RdbmsDriverType::sqlite, std::string(kAddMissingPrimaryKeys_sqlite)},
        {nx::sql::RdbmsDriverType::unknown, std::string(kAddMissingPrimaryKeys_mysql)} //< default.
    });

    schemaUpdater.updateStruct(queryContext);
}

bool DbStructureUpdater::dbVersionTableExists(QueryContext* queryContext)
{
    return queryContext->connection()->tableExist("db_version_data");
}

void DbStructureUpdater::syncDbVersionAndAppliedScriptsTables(QueryContext* queryContext)
{
    const int dbVersion = fetchLegacyDbVersion(queryContext);
    const auto appliedScripts = m_schemaUpdater.fetchAppliedSchemaUpdates(queryContext);

    NX_DEBUG(this, "Syncing DB scheme %1 applied scripts. Legacy version: %2, scripts: %3",
        m_schemaName, dbVersion, appliedScripts);

    for (int i = 1; i < dbVersion; ++i)
    {
        const auto scriptName = std::to_string(i);
        if (appliedScripts.count(scriptName) > 0)
            continue;

        NX_DEBUG(this, "DB scheme %1. Carrying over script %2 from legacy history",
            m_schemaName, scriptName);

        auto query = queryContext->connection()->createQuery();
        query->prepare(
            "INSERT INTO db_schema_applied_scripts(schema_name, script_name) "
            "VALUES (?, ?)");
        query->addBindValue(m_schemaName);
        query->addBindValue(scriptName);
        query->exec();
    }
}

int DbStructureUpdater::fetchLegacyDbVersion(QueryContext* queryContext)
{
    try
    {
        // Implying that schema_name field is present.
        auto query = queryContext->connection()->createQuery();
        query->prepare(
            "SELECT db_version "
            "FROM db_version_data "
            "WHERE schema_name=?");
        query->addBindValue(m_schemaName);
        query->exec();

        return query->next() ? query->value(0).toInt() : 0;
    }
    catch (const nx::sql::Exception&)
    {
        // Ignoring exception. Maybe db_version_data table is that old that it does not
        // contain schema_name field?
    }

    auto query = queryContext->connection()->createQuery();
    query->prepare(
        "SELECT db_version "
        "FROM db_version_data");
    query->exec();

    return query->next() ? query->value(0).toInt() : 0;
}

} // namespace nx::sql
