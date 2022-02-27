// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_structure_updater.h"

#include <nx/utils/log/log.h>

#include "async_sql_query_executor.h"
#include "sql_query_execution_helper.h"
#include "query.h"
#include "types.h"

namespace nx::sql {

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

void DbStructureUpdater::setInitialVersion(unsigned int version)
{
    m_schemaUpdater.setInitialVersion(version);
}

void DbStructureUpdater::addUpdateScript(QByteArray updateScript)
{
    m_schemaUpdater.addUpdateScript(std::move(updateScript));
}

void DbStructureUpdater::addUpdateScript(const std::string_view& updateScript)
{
    m_schemaUpdater.addUpdateScript(QByteArray(updateScript.data(), (int) updateScript.size()));
}

void DbStructureUpdater::addUpdateScript(const char* updateScript)
{
    m_schemaUpdater.addUpdateScript(QByteArray(updateScript));
}

void DbStructureUpdater::addUpdateScript(
    std::map<RdbmsDriverType, QByteArray> scriptByDbType)
{
    m_schemaUpdater.addUpdateScript(std::move(scriptByDbType));
}

void DbStructureUpdater::addUpdateScript(
    std::map<RdbmsDriverType, std::string> scriptByDbType)
{
    std::map<RdbmsDriverType, QByteArray> converted;
    std::transform(
        scriptByDbType.begin(), scriptByDbType.end(),
        std::inserter(converted, converted.end()),
        [](const auto& val) { return std::make_pair(val.first, QByteArray::fromStdString(val.second)); });
    addUpdateScript(std::move(converted));
}

void DbStructureUpdater::addUpdateFunc(UpdateFunc dbUpdateFunc)
{
    m_schemaUpdater.addUpdateFunc(std::move(dbUpdateFunc));
}

void DbStructureUpdater::addFullSchemaScript(
    unsigned int version,
    QByteArray createSchemaScript)
{
    m_schemaUpdater.addFullSchemaScript(version, std::move(createSchemaScript));
}

unsigned int DbStructureUpdater::maxKnownVersion() const
{
    return m_schemaUpdater.maxKnownVersion();
}

void DbStructureUpdater::setVersionToUpdateTo(unsigned int version)
{
    return m_schemaUpdater.setVersionToUpdateTo(version);
}

void DbStructureUpdater::updateStructSyncThrow()
{
    using namespace std::placeholders;

    try
    {
        m_queryExecutor->executeUpdateQuerySync(
            std::bind(&DbStructureUpdater::updateStruct, this, _1));
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
    }
    catch (const sql::Exception&)
    {
        return false;
    }

    return true;
}

void DbStructureUpdater::updateStruct(nx::sql::QueryContext* queryContext)
{
    updateDbToMultipleSchema(queryContext);
    m_schemaUpdater.updateStruct(queryContext);
}

bool DbStructureUpdater::exists() const
{
    try
    {
        return m_queryExecutor->executeUpdateQuerySync(
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
            SELECT db_version FROM db_version_data
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
    query->prepare(
        R"sql(
            UPDATE db_version_data SET schema_name = ?
            WHERE schema_name = ?
        )sql");

    query->addBindValue(newName);
    query->addBindValue(oldName);
    query->exec();
}

void DbStructureUpdater::updateDbToMultipleSchema(QueryContext* queryContext)
{
    if (!dbVersionTableExists(queryContext))
    {
        createInitialSchema(queryContext);
    }
    else if (!dbVersionTableSupportsMultipleSchemas(queryContext))
    {
        updateDbVersionTable(queryContext);
        setDbSchemaName(queryContext, m_schemaName);
    }

    // db_version_data table is present and supports multiple schemas. Nothing to do here.
}

bool DbStructureUpdater::dbVersionTableExists(QueryContext* queryContext)
{
    SqlQuery selectDbVersionQuery(queryContext->connection());
    try
    {
        selectDbVersionQuery.prepare(R"sql(
            SELECT count(*) FROM db_version_data
        )sql");
        selectDbVersionQuery.exec();
        return true;
    }
    catch (const Exception& e)
    {
        NX_VERBOSE(this, nx::format("db_version_data table presence check failed. %1").arg(e.what()));
        // It is better to check error type here, but qt does not always return proper error code.
        return false;
    }
}

void DbStructureUpdater::createInitialSchema(QueryContext* queryContext)
{
    NX_VERBOSE(this, nx::format("Creating maintenance structure"));

    SqlQuery createInitialSchemaQuery(queryContext->connection());
    createInitialSchemaQuery.prepare(R"sql(
        CREATE TABLE db_version_data (
            schema_name VARCHAR(128) NOT NULL PRIMARY KEY,
            db_version INTEGER NOT NULL DEFAULT 0
        );
    )sql");
    createInitialSchemaQuery.exec();
}

bool DbStructureUpdater::dbVersionTableSupportsMultipleSchemas(QueryContext* queryContext)
{
    SqlQuery selectDbVersionQuery(queryContext->connection());
    try
    {
        selectDbVersionQuery.prepare(R"sql(
            SELECT db_version, schema_name FROM db_version_data
        )sql");
        selectDbVersionQuery.exec();
        return true;
    }
    catch (const Exception& e)
    {
        NX_VERBOSE(this, nx::format("db_version_data.schema_name field presence check failed. %1").arg(e.what()));
        // It is better to check error type here, but qt does not always return proper error code.
        return false;
    }
}

void DbStructureUpdater::updateDbVersionTable(QueryContext* queryContext)
{
    NX_VERBOSE(this, nx::format("Updating db_version_data table"));

    try
    {
        SqlQuery::exec(
            queryContext->connection(),
            R"sql(
                ALTER TABLE db_version_data RENAME TO db_version_data_old;
            )sql");

        SqlQuery::exec(
            queryContext->connection(),
            R"sql(
                CREATE TABLE db_version_data (
                    schema_name VARCHAR(128) NOT NULL PRIMARY KEY,
                    db_version INTEGER NOT NULL DEFAULT 0
                );
            )sql");

        SqlQuery::exec(
            queryContext->connection(),
            R"sql(
                INSERT INTO db_version_data(schema_name, db_version)
                SELECT "", db_version FROM db_version_data_old
            )sql");

        SqlQuery::exec(
            queryContext->connection(),
            R"sql(
                DROP TABLE db_version_data_old;
            )sql");
    }
    catch (const Exception& e)
    {
        NX_DEBUG(this, nx::format("db_version_data table update failed. %1").arg(e.what()));
        throw;
    }
}

void DbStructureUpdater::setDbSchemaName(
    QueryContext* queryContext,
    const std::string& schemaName)
{
    SqlQuery setDbSchemaNameQuery(queryContext->connection());
    setDbSchemaNameQuery.prepare(R"sql(
        UPDATE db_version_data SET schema_name=:schemaName
    )sql");
    setDbSchemaNameQuery.bindValue(":schemaName", QString::fromStdString(schemaName));
    setDbSchemaNameQuery.exec();
}

} // namespace nx::sql
