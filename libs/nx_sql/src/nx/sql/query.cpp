// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query.h"

#include <chrono>

#include <QtSql/QSqlError>

#include <nx/utils/log/log.h>

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

void SqlQuery::prepare(const std::string_view& query)
{
    if (!m_sqlQuery.prepare(QString::fromUtf8(query.data(), query.size())))
    {
        NX_DEBUG(this, "Error preparing query %1. %2", query, m_sqlQuery.lastError().text());
        throw Exception(
            getLastErrorCode(),
            m_sqlQuery.lastError().text().toStdString());
    }
}

void SqlQuery::addBindValue(const QVariant& value) noexcept
{
    m_sqlQuery.addBindValue(value);
}

void SqlQuery::addBindValue(const std::string_view& value) noexcept
{
    m_sqlQuery.addBindValue(QString::fromUtf8(value.data(), value.size()));
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
    const std::string_view& placeholder,
    const std::string_view& value) noexcept
{
    bindValue(
        QString::fromUtf8(placeholder.data(), placeholder.size()),
        QString::fromUtf8(value.data(), value.size()));
}

void SqlQuery::bindValue(int pos, const std::string_view& value) noexcept
{
    bindValue(pos, QString::fromUtf8(value.data(), value.size()));
}

void SqlQuery::exec()
{
    using namespace std::chrono;

    const auto t0 = steady_clock::now();

    const auto ok = m_sqlQuery.exec();

    if (ok)
    {
        NX_TRACE(this, "Query %1 completed in %2", m_sqlQuery.lastQuery(),
            floor<milliseconds>(steady_clock::now() - t0));
    }
    else
    {
        NX_TRACE(this, "Query %1 failed. %2", m_sqlQuery.lastQuery(),
            m_sqlQuery.lastError().text());

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
    query.prepare(std::string_view(queryText.data(), queryText.size()));
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
