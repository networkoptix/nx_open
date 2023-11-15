// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detail_db_structure_updater.h"

#include <functional>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "../async_sql_query_executor.h"
#include "../sql_query_execution_helper.h"

namespace nx::sql::detail {

namespace {

struct SqlReplacementContext
{
    std::string key;
    std::string defaultValue;
    /** map<Sql driver, replacement>. */
    std::map<RdbmsDriverType, std::string> replacementsByDriverName;
};

using ReplacementsDictionary = std::vector<SqlReplacementContext>;

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

DbStructureUpdater::DbStructureUpdater(const std::string& schemaName):
    m_schemaName(schemaName)
{
}

bool DbStructureUpdater::addUpdateScript(std::string name, std::string updateScript)
{
    if (!m_updateScriptNames.insert(name).second)
        return false;

    m_updateScripts.emplace_back(std::move(name), std::move(updateScript));
    return true;
}

bool DbStructureUpdater::addUpdateScript(
    std::string name,
    std::map<RdbmsDriverType, std::string> scriptByDbType)
{
    if (!m_updateScriptNames.insert(name).second)
        return false;

    m_updateScripts.emplace_back(std::move(name), std::move(scriptByDbType));
    return true;
}

bool DbStructureUpdater::addUpdateFunc(std::string name, UpdateFunc dbUpdateFunc)
{
    if (!m_updateScriptNames.insert(name).second)
        return false;

    m_updateScripts.emplace_back(std::move(name), std::move(dbUpdateFunc));
    return true;
}

void DbStructureUpdater::updateStruct(QueryContext* const queryContext)
{
    std::set<std::string> appliedScripts = fetchAppliedSchemaUpdates(queryContext);
    NX_DEBUG(this, "The DB scheme %1 has the following scripts applied: %2",
        m_schemaName, appliedScripts);

    for (const auto& update: m_updateScripts)
    {
        if (appliedScripts.count(update.name) == 0)
            applyUpdate(queryContext, update);
    }
}

std::set<std::string> DbStructureUpdater::fetchAppliedSchemaUpdates(
    nx::sql::QueryContext* const queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(
        "SELECT script_name "
        "FROM db_schema_applied_scripts "
        "WHERE schema_name=?");
    query->bindValue(0, m_schemaName);
    query->exec();

    std::set<std::string> scripts;
    while (query->next())
        scripts.insert(query->value(0).toString().toStdString());
    return scripts;
}

void DbStructureUpdater::applyUpdate(
    nx::sql::QueryContext* const queryContext,
    const DbUpdate& dbUpdate)
{
    NX_INFO(this, "Applying schema %1 update %2", m_schemaName, dbUpdate.name);

    if (!execDbUpdate(queryContext, dbUpdate))
    {
        NX_INFO(this, "Error applying schema %1 update %2", m_schemaName, dbUpdate.name);
        throw nx::sql::Exception(nx::sql::DBResultCode::statementError);
    }

    recordSuccessfulUpdate(queryContext, dbUpdate);
}

bool DbStructureUpdater::execDbUpdate(
    nx::sql::QueryContext* const queryContext,
    const DbUpdate& dbUpdate)
{
    if (!dbUpdate.dbTypeToSqlScript.empty())
    {
        if (!execStructureUpdateTask(queryContext, dbUpdate.dbTypeToSqlScript))
            return false;
    }

    if (dbUpdate.func)
    {
        NX_DEBUG(this, "DB schema %1. Applying update by invoking an external function", m_schemaName);
        if (auto result = dbUpdate.func(queryContext); result != nx::sql::DBResultCode::ok)
        {
            NX_WARNING(this, "Error executing update function. %1", result);
            return false;
        }
    }

    return true;
}

bool DbStructureUpdater::execStructureUpdateTask(
    nx::sql::QueryContext* const queryContext,
    const std::map<RdbmsDriverType, std::string>& dbTypeToScript)
{
    const auto driverType = queryContext->connection()->driverType();

    auto selectedScriptIter = selectSuitableScript(dbTypeToScript, driverType);
    if (selectedScriptIter == dbTypeToScript.end())
    {
        NX_DEBUG(this, "DB schema %1. Could not find script version for DB %2. Aborting...",
            m_schemaName, driverType);
        return false;
    }

    return execSqlScript(
        queryContext,
        selectedScriptIter->second,
        selectedScriptIter->first);
}

std::map<RdbmsDriverType, std::string>::const_iterator DbStructureUpdater::selectSuitableScript(
    const std::map<RdbmsDriverType, std::string>& dbTypeToScript,
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
    nx::sql::QueryContext* queryContext,
    const std::string& sqlScript,
    RdbmsDriverType sqlScriptDialect)
{
    std::string scriptText;
    if (sqlScriptDialect == RdbmsDriverType::unknown)
        scriptText = fixSqlDialect(sqlScript, queryContext->connection()->driverType());
    else
        scriptText = sqlScript;

    NX_DEBUG(this, "DB schema %1. Executing SQL script \n%2", m_schemaName, scriptText);

    nx::sql::SqlQueryExecutionHelper::execSQLScript(queryContext, scriptText);

    return true;
}

std::string DbStructureUpdater::fixSqlDialect(
    const std::string& originalScript,
    RdbmsDriverType targetDialect)
{
    std::string script = originalScript;
    for (const auto& replacementCtx: kSqlReplacements)
    {
        const auto it = replacementCtx.replacementsByDriverName.find(targetDialect);
        if (it != replacementCtx.replacementsByDriverName.end())
            script = nx::utils::replace(script, replacementCtx.key, it->second);
        else
            script = nx::utils::replace(script, replacementCtx.key, replacementCtx.defaultValue);
    }

    return script;
}

namespace {

/**
 * Replaces each occurence of token in text with subsequent element of args.
 * Example:
 * substituteArgs("? ? ?", "?", "one", "two", "three")
 * provides
 * "one two three"
 */
template<typename... FutherStrs>
std::string substituteArgs(
    const std::string& text,
    const std::string_view& token,
    const std::string_view& str,
    const FutherStrs&... futherStrs)
{
    auto pos = text.find(token);
    if (pos == std::string::npos)
        return text; //< Nothing left to replace.

    std::string result = text;
    result.replace(pos, token.size(), str);
    if constexpr (sizeof...(FutherStrs) == 0)
        return result; //< No replacements left.
    else
        return substituteArgs(result, token, futherStrs...);
}

} // namespace

void DbStructureUpdater::recordSuccessfulUpdate(
    nx::sql::QueryContext* const queryContext,
    const DbUpdate& dbUpdate)
{
    static constexpr char kSqlText[] =
        "INSERT INTO db_schema_applied_scripts(schema_name, script_name) VALUES (?, ?)";

    auto query = queryContext->connection()->createQuery();
    query->prepare(kSqlText);
    query->addBindValue(m_schemaName);
    query->addBindValue(dbUpdate.name);
    query->exec();

    NX_DEBUG(this, "DB schema %1. Saving update result \n%2\n", m_schemaName,
        substituteArgs(kSqlText, "?",  "\"" + m_schemaName + "\"", "\"" + dbUpdate.name + "\""));
}

} // namespace nx::sql::detail
