#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtSql/QSqlDatabase>

namespace nx {
namespace utils {
namespace db {

class SqlQueryExecutionHelper
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
};

} // namespace db
} // namespace utils
} // namespace nx
