#include "sql_query_execution_helper.h"

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

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
            result << QByteArray::fromRawData(prevPtr, curPtr - prevPtr);
            prevPtr = curPtr + 1;
        }
    }

    return result;
}

bool validateParams(const QSqlQuery& query)
{
    for (const auto& value: query.boundValues().values())
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

bool SqlQueryExecutionHelper::execSQLQuery(QSqlQuery *query, const char* details)
{
    NX_EXPECT(validateParams(*query));
    if (!query->exec())
    {
        auto error = query->lastError();
        NX_ASSERT(error.type() != QSqlError::StatementError,
            error.text() + lit(":\n") + query->lastQuery());
        NX_LOG(lit("%1 %2").arg(QLatin1String(details)).arg(error.text()), cl_logERROR);

        return false;
    }
    return true;
}

bool SqlQueryExecutionHelper::prepareSQLQuery(QSqlQuery *query, const QString &queryStr, const char* details)
{
    if (!query->prepare(queryStr))
    {
        NX_LOG(lit("%1 %2").arg(QLatin1String(details)).arg(query->lastError().text()), cl_logERROR);
        NX_ASSERT(false, details, "Unable to prepare SQL query");
        return false;
    }
    return true;
}

bool SqlQueryExecutionHelper::execSQLScript(
    const QByteArray& scriptData,
    QSqlDatabase& database)
{
    QList<QByteArray> commands = quotedSplit(scriptData);
#ifdef DB_DEBUG
    int n = commands.size();
    qDebug() << "creating db" << n << "commands queued";
    int i = 0;
#endif // DB_DEBUG
    for (const QByteArray& singleCommand : commands)
    {
#ifdef DB_DEBUG
        qDebug() << QString(QLatin1String("processing command %1 of %2")).arg(++i).arg(n);
#endif // DB_DEBUG
        QString command = QString::fromUtf8(singleCommand).trimmed();
        if (command.isEmpty())
            continue;
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
        NX_LOG(lit("Error while executing SQL file %1").arg(fileName), cl_logERROR);
        return false;
    }
    return true;
}
