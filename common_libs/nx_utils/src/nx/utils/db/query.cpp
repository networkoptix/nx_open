#include "query.h"

#include <QtSql/QSqlError>

namespace nx {
namespace utils {
namespace db {

SqlQuery::SqlQuery(QSqlDatabase connection):
    m_sqlQuery(connection)
{
}

void SqlQuery::setForwardOnly(bool val)
{
    m_sqlQuery.setForwardOnly(val);
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

bool SqlQuery::next()
{
    return m_sqlQuery.next();
}

QVariant SqlQuery::value(int index) const
{
    return m_sqlQuery.value(index);
}

QVariant SqlQuery::value(const QString& name) const
{
    return m_sqlQuery.value(name);
}

QSqlRecord SqlQuery::record()
{
    return m_sqlQuery.record();
}

QSqlQuery& SqlQuery::impl()
{
    return m_sqlQuery;
}

const QSqlQuery& SqlQuery::impl() const
{
    return m_sqlQuery;
}

void SqlQuery::exec(QSqlDatabase connection, const QByteArray& queryText)
{
    SqlQuery query(connection);
    query.prepare(queryText);
    query.exec();
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
} // namespace utils
} // namespace nx
