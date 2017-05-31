#include "query.h"

#include <QtSql/QSqlError>

namespace nx {
namespace db {

SqlQuery::SqlQuery(QSqlDatabase connection):
    m_sqlQuery(connection)
{
}

void SqlQuery::prepare(const QString& query)
{
    if (!m_sqlQuery.prepare(query))
    {
        throw Exception(
            getLastErrorCode(),
            m_sqlQuery.lastError().text().toStdString());
    }
}

void SqlQuery::bindValue(const QString& placeholder, const QVariant& value) noexcept
{
    m_sqlQuery.bindValue(placeholder, value);
}

void SqlQuery::bindValue(int pos, const QVariant& value) noexcept
{
    m_sqlQuery.bindValue(pos, value);
}

void SqlQuery::exec()
{
    if (!m_sqlQuery.exec())
    {
        throw Exception(
            getLastErrorCode(),
            m_sqlQuery.lastError().text().toStdString());
    }
}

DBResult SqlQuery::getLastErrorCode()
{
    switch (m_sqlQuery.lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;
        case QSqlError::ConnectionError:
            return DBResult::connectionError;
        default:
            return DBResult::ioError;
    }
}

} // namespace db
} // namespace nx
