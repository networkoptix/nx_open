#include "db_structure_updater.h"

#include "async_sql_query_executor.h"
#include "sql_query_execution_helper.h"
#include "query.h"
#include "types.h"

namespace nx {
namespace utils {
namespace db {

DbStructureUpdater::DbStructureUpdater(
    const std::string& schemaName,
    AbstractAsyncSqlQueryExecutor* const queryExecutor)
    :
    m_schemaUpdater(schemaName, queryExecutor),
    m_schemaName(schemaName),
    m_queryExecutor(queryExecutor)
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

void DbStructureUpdater::addUpdateScript(
    std::map<RdbmsDriverType, QByteArray> scriptByDbType)
{
    m_schemaUpdater.addUpdateScript(std::move(scriptByDbType));
}

void DbStructureUpdater::addUpdateFunc(DbUpdateFunc dbUpdateFunc)
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

bool DbStructureUpdater::updateStructSync()
{
    using namespace std::placeholders;

    try
    {
        m_queryExecutor->executeUpdateQuerySync(
            std::bind(&DbStructureUpdater::updateDbToMultipleSchema, this, _1));
    }
    catch (db::Exception /*e*/)
    {
        return false;
    }

    return m_schemaUpdater.updateStructSync();
}

void DbStructureUpdater::updateDbToMultipleSchema(QueryContext* queryContext)
{
    if (!isDbVersionTableExists(queryContext))
    {
        createInitialSchema(queryContext);
    }
    else if (!isDbVersionTableSupportsMultipleSchemas(queryContext))
    {
        updateDbVersionTable(queryContext);
        setDbSchemaNameTo(queryContext, m_schemaName);
    }
 
    // db_version_data table is present and supports multiple schemas. Nothing to do here.
}

bool DbStructureUpdater::isDbVersionTableExists(QueryContext* queryContext)
{
    SqlQuery selectDbVersionQuery(*queryContext->connection());
    try
    {
        selectDbVersionQuery.prepare(R"sql(
            SELECT count(*) FROM db_version_data
        )sql");
        selectDbVersionQuery.exec();
        return true;
    }
    catch (Exception /*e*/)
    {
        // It is better to check error type here, but qt does not always return proper error code.
        return false;
    }
}

void DbStructureUpdater::createInitialSchema(QueryContext* queryContext)
{
    SqlQuery createInitialSchemaQuery(*queryContext->connection());
    createInitialSchemaQuery.prepare(R"sql(
        CREATE TABLE db_version_data (
            schema_name VARCHAR2(1024) NOT NULL PRIMARY KEY,
            db_version INTEGER NOT NULL DEFAULT 0
        );
    )sql");
    createInitialSchemaQuery.exec();
}

bool DbStructureUpdater::isDbVersionTableSupportsMultipleSchemas(QueryContext* queryContext)
{
    SqlQuery selectDbVersionQuery(*queryContext->connection());
    try
    {
        selectDbVersionQuery.prepare(R"sql(
            SELECT db_version, schema_name FROM db_version_data
        )sql");
        selectDbVersionQuery.exec();
        return true;
    }
    catch (Exception /*e*/)
    {
        // It is better to check error type here, but qt does not always return proper error code.
        return false;
    }
}

void DbStructureUpdater::updateDbVersionTable(QueryContext* queryContext)
{
    SqlQuery::exec(
        *queryContext->connection(),
        R"sql(
            ALTER TABLE db_version_data RENAME TO db_version_data_old;
        )sql");

    SqlQuery::exec(
        *queryContext->connection(),
        R"sql(
            CREATE TABLE db_version_data (
                schema_name VARCHAR2(1024) NOT NULL PRIMARY KEY,
                db_version INTEGER NOT NULL DEFAULT 0
            );
        )sql");

    SqlQuery::exec(
        *queryContext->connection(),
        R"sql(
            INSERT INTO db_version_data(schema_name, db_version)
            SELECT "", db_version FROM db_version_data_old
        )sql");
}

void DbStructureUpdater::setDbSchemaNameTo(
    QueryContext* queryContext,
    const std::string& schemaName)
{
    SqlQuery setDbSchemaNameQuery(*queryContext->connection());
    setDbSchemaNameQuery.prepare(R"sql(
        UPDATE db_version_data SET schema_name=:schemaName
    )sql");
    setDbSchemaNameQuery.bindValue(":schemaName", QString::fromStdString(schemaName));
    setDbSchemaNameQuery.exec();
}

} // namespace db
} // namespace utils
} // namespace nx
