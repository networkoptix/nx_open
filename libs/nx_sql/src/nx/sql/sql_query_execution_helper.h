#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

#include <nx/utils/uuid.h>

#include "abstract_db_connection.h"

namespace nx::sql {

class NX_SQL_API SqlQueryExecutionHelper
{
public:
    static bool execSQLQuery(const QString& query, QSqlDatabase& database, const char* details);

    static bool execSQLQuery(QSqlQuery *query, const char* details);
    static bool prepareSQLQuery(QSqlQuery *query, const QString &queryStr, const char* details);
    /**
     * @param scriptData SQL commands separated with ;
     */
    static bool execSQLScript(const QByteArray& scriptData, QSqlDatabase& database);
    /** Reads file fileName and calls QnDbHelper::execSQLScript. */
    static bool execSQLFile(const QString& fileName, QSqlDatabase& database);

    static bool execSQLScript(const QByteArray& scriptData, AbstractDbConnection& dbConnection);
    static bool execSQLFile(const QString& fileName, AbstractDbConnection& dbConnection);

    /** Bind uuid to a query parameter. By default, null id is bound as NULL (param: optional). */
    static void bindId(QSqlQuery* query,
        const QString& parameter,
        const QnUuid& id,
        bool optional = true);
};

} // namespace nx::sql
