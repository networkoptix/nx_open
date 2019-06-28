#include "query.h"

#include <QtSql/QSqlError>

#include "abstract_db_connection.h"

namespace nx::sql {

SqlQuery::SqlQuery(QSqlDatabase connection):
    m_sqlQuery(connection)
{
}

SqlQuery::SqlQuery(AbstractDbConnection* connection):
    m_sqlQuery(*connection->qtSqlConnection())
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

void SqlQuery::addBindValue(const QVariant& value) noexcept
{
    m_sqlQuery.addBindValue(value);
}

void SqlQuery::bindValue(const QString& placeholder, const QVariant& value) noexcept
{
    m_sqlQuery.bindValue(placeholder, value);
}

void SqlQuery::bindValue(int pos, const QVariant& value) noexcept
{
    m_sqlQuery.bindValue(pos, value);
}

void SqlQuery::bindValue(
    const std::string& placeholder,
    const std::string& value) noexcept
{
    bindValue(
        QString::fromStdString(placeholder),
        QString::fromStdString(value));
}

void SqlQuery::bindValue(int pos, const std::string& value) noexcept
{
    bindValue(pos, QString::fromStdString(value));
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

QVariant SqlQuery::lastInsertId() const
{
    return m_sqlQuery.lastInsertId();
}

int SqlQuery::numRowsAffected() const
{
    return m_sqlQuery.numRowsAffected();
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

void SqlQuery::exec(AbstractDbConnection* connection, const QByteArray& queryText)
{
    exec(*connection->qtSqlConnection(), queryText);
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


/**
* Returns more detailed result code if appropriate. Otherwise returns initial one.
*/
//DBResult detailResultCode(AbstractDbConnection* const connection, DBResult result) const;

//DBResult BaseExecutor::detailResultCode(
//    AbstractDbConnection* const connection,
//    DBResult result) const
//{
//    if (result != DBResult::ioError)
//        return result;
//    switch (connection->lastError().type())
//    {
//        case QSqlError::StatementError:
//            return DBResult::statementError;
//        case QSqlError::ConnectionError:
//            return DBResult::connectionError;
//        default:
//            return result;
//    }
//}

} // namespace nx::sql
