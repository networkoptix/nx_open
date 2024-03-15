// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sql_query_execution_helper.h"

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

namespace nx::sql {

namespace {

QList<QByteArray> quotedSplit(const QByteArray& data)
{
    QList<QByteArray> result;
    const char* curPtr = data.data();
    const char* prevPtr = curPtr;
    const char* end = curPtr + data.size();
    bool inQuote1 = false;
    bool inQuote2 = false;
    for (; curPtr < end; ++curPtr)
    {
        if (*curPtr == '\'')
            inQuote1 = !inQuote1;
        else if (*curPtr == '\"')
            inQuote2 = !inQuote2;
        else if (*curPtr == ';' && !inQuote1 && !inQuote2)
        {
            //*curPtr = 0;
            result << QByteArray::fromRawData(prevPtr, (int) (curPtr - prevPtr));
            prevPtr = curPtr + 1;
        }
    }

    return result;
}

static bool validateParams(const QSqlQuery& query)
{
    const auto values = query.boundValues();
    for (const auto& value: values)
    {
        if (!value.isValid())
            return false;
    }
    return true;
}

} // namespace

bool SqlQueryExecutionHelper::execSQLQuery(const QString& queryStr, QSqlDatabase& database, const char* details)
{
    QSqlQuery query(database);
    return prepareSQLQuery(&query, queryStr, details) && execSQLQuery(&query, details);
}

bool SqlQueryExecutionHelper::execSQLQuery(QSqlQuery* query, const char* details)
{
    NX_ASSERT_HEAVY_CONDITION(validateParams(*query), "Failure SQL in %2: %3", details, query->lastQuery());
    if (query->exec())
    {
        NX_VERBOSE(NX_SCOPE_TAG, "%1 executed SQL query:\n%2", details, query->lastQuery());
        return true;
    }

    auto error = query->lastError();
    // Assert only on statement error type as other errors are not our code responsibility.
    if (NX_ASSERT(error.type() != QSqlError::StatementError,
        "%1 to execute SQL query in %2: %3", error, details, query->lastQuery()))
    {
        NX_DEBUG(NX_SCOPE_TAG, "%1 to execute SQL query in %2: %3",
            error, details, query->lastQuery());
    }
    return false;
}

bool SqlQueryExecutionHelper::prepareSQLQuery(
    QSqlQuery* query, const QString& queryStr, const char* details)
{
    return NX_ASSERT(query->prepare(queryStr),
        "%1 to prepare SQL query in %2: %3", query->lastError(), details, query->lastQuery());
}

bool SqlQueryExecutionHelper::execSQLScript(const QByteArray& scriptData, QSqlDatabase& database)
{
    QTextStream text(scriptData);
    QString scriptWithoutComments;
    QTextStream out(&scriptWithoutComments);
    QString line;
    while (text.readLineInto(&line))
    {
        if (!line.startsWith("--"))
            out << line << Qt::endl;
    }

    QByteArray dataWithoutComments = scriptWithoutComments.toUtf8();
    QList<QByteArray> commands = quotedSplit(dataWithoutComments);
    size_t currentCommand = 1;
    for (const QByteArray& singleCommand : commands)
    {
        QString command = QString::fromUtf8(singleCommand).trimmed();
        if (command.isEmpty())
            continue;
        NX_VERBOSE(NX_SCOPE_TAG, "Executing %1/%2 command from script:\n%3",
            currentCommand++, commands.size(), command);
        QSqlQuery ddlQuery(database);
        if (!prepareSQLQuery(&ddlQuery, command, Q_FUNC_INFO))
            return false;
        if (!execSQLQuery(&ddlQuery, Q_FUNC_INFO))
            return false;
    }
    return true;
}

bool SqlQueryExecutionHelper::execSQLFile(
    const QString& fileName,
    QSqlDatabase& database)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    if (data.isEmpty())
        return true;

    if (!execSQLScript(data, database))
    {
        NX_ASSERT(false, "Error while executing SQL file %1", fileName);
        return false;
    }
    NX_DEBUG(NX_SCOPE_TAG, "Successfully executed SQL file %1", fileName);
    return true;
}

bool SqlQueryExecutionHelper::execSQLScript(
    const QByteArray& scriptData,
    AbstractDbConnection& dbConnection)
{
    return execSQLScript(scriptData, *dbConnection.qtSqlConnection());
}

bool SqlQueryExecutionHelper::execSQLFile(
    const QString& fileName,
    AbstractDbConnection& dbConnection)
{
    return execSQLFile(fileName, *dbConnection.qtSqlConnection());
}

void SqlQueryExecutionHelper::execSQLScript(
    nx::sql::QueryContext* queryContext,
    const std::string_view& script)
{
    nx::utils::split(
        script,
        ';',
        [queryContext](auto stmt)
        {
            stmt = nx::utils::trim(stmt);
            if (stmt.empty())
                return;

            auto query = queryContext->connection()->createQuery();
            query->prepare(stmt);
            query->exec();
        },
        nx::utils::GroupToken::doubleQuotes);
}

void SqlQueryExecutionHelper::bindId(QSqlQuery* query,
    const QString& parameter,
    const nx::Uuid& id,
    bool optional)
{
    if (optional && id.isNull())
        query->bindValue(parameter, QByteArray());
    else
        query->bindValue(parameter, id.toRfc4122());
}

} // namespace nx::sql
