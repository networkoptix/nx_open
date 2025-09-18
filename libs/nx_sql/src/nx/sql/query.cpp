// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query.h"

#include <chrono>

#include <QtSql/QSqlError>

#include <nx/utils/log/log.h>

#include "abstract_db_connection.h"
#include "db_statistics_collector.h"

namespace nx::sql {

SqlQuery::SqlQuery(QSqlDatabase connection):
    m_sqlQuery(connection),
    m_driverType(rdbmsDriverTypeFromString(connection.driverName().toStdString()))
{
}

SqlQuery::SqlQuery(AbstractDbConnection* connection):
    m_sqlQuery(*connection->qtSqlConnection()),
    m_driverType(
        rdbmsDriverTypeFromString(connection->qtSqlConnection()->driverName().toStdString()))
{
}

SqlQuery::SqlQuery(QSqlDatabase connection, StatisticsCollector* statisticsCollector)
    :
    m_sqlQuery(connection),
    m_statisticsCollector(statisticsCollector),
    m_driverType(rdbmsDriverTypeFromString(connection.driverName().toStdString()))
{
}

void SqlQuery::setForwardOnly(bool val)
{
    m_sqlQuery.setForwardOnly(val);
}

void SqlQuery::prepare(std::string_view query)
{
    m_unpreparedQuery.reset();

    if (!shouldPrepare(query))
    {
        m_unpreparedQuery = QString::fromUtf8(query);
        return;
    }

    if (!m_sqlQuery.prepare(QString::fromUtf8(query.data(), query.size())))
    {
        NX_DEBUG(this, "Error preparing query %1. %2", query, m_sqlQuery.lastError().text());
        throw Exception(getLastError());
    }
}

void SqlQuery::addBindValue(const QVariant& value) noexcept
{
    m_sqlQuery.addBindValue(value);
}

void SqlQuery::addBindValue(std::string_view value) noexcept
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
    std::string_view placeholder,
    std::string_view value) noexcept
{
    bindValue(
        QString::fromUtf8(placeholder.data(), placeholder.size()),
        QString::fromUtf8(value.data(), value.size()));
}

void SqlQuery::bindValue(int pos, std::string_view value) noexcept
{
    bindValue(pos, QString::fromUtf8(value.data(), value.size()));
}

void SqlQuery::exec()
{
    exec(std::nullopt);
}

void SqlQuery::exec(std::string_view query)
{
    exec(std::optional<std::string_view>(query));
}

void SqlQuery::exec(const std::optional<std::string_view>& query)
{
    using namespace std::chrono;

    const auto t0 = steady_clock::now();

    bool ok;

    if (query)
    {
        ok = m_sqlQuery.exec(QString::fromUtf8(query->data(), query->size()));
    }
    else if (m_unpreparedQuery)
    {
        ok = m_sqlQuery.exec(*m_unpreparedQuery);
    }
    else
    {
        ok = m_sqlQuery.exec();
    }

    auto executionTime = floor<milliseconds>(steady_clock::now() - t0);

    if (m_statisticsCollector)
        m_statisticsCollector->recordQuery(m_sqlQuery.lastQuery().toStdString(), executionTime);

    if (ok)
    {
        NX_TRACE(this, "Query %1 completed in %2", m_sqlQuery.lastQuery(), executionTime);
    }
    else
    {
        NX_TRACE(this, "Query %1 failed. %2", m_sqlQuery.lastQuery(),
            m_sqlQuery.lastError().text());

        throw Exception(getLastError());
    }
}

bool SqlQuery::shouldPrepare(std::string_view query)
{
    auto stmt = QString::fromUtf8(query);
    return !stmt.startsWith("ALTER", Qt::CaseInsensitive)
        && !stmt.startsWith("CREATE", Qt::CaseInsensitive)
        && !stmt.startsWith("DROP", Qt::CaseInsensitive);
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

DBResult SqlQuery::getLastError()
{
    DBResult res;

    const auto lastError = m_sqlQuery.lastError();

    switch (lastError.type())
    {
        case QSqlError::StatementError:
            res.code = DBResultCode::statementError;
            break;

        case QSqlError::ConnectionError:
            res.code = DBResultCode::connectionError;
            break;

        default:
            res.code = DBResultCode::ioError;
    }

    // Trying to add more details to the result code.
    if (m_driverType == RdbmsDriverType::sqlite)
    {
        const auto nativeErrorCode = m_sqlQuery.lastError().nativeErrorCode();
        if (nativeErrorCode == "2067") //< SQLITE_CONSTRAINT_UNIQUE
            res.code = DBResultCode::uniqueConstraintViolation;
    }
    else if (m_driverType == RdbmsDriverType::mysql)
    {
        const auto nativeErrorCode = m_sqlQuery.lastError().nativeErrorCode();
        if (nativeErrorCode == "1062") //< ERROR 1062 (23000): Duplicate entry
            res.code = DBResultCode::uniqueConstraintViolation;
    }

    res.text = lastError.text().toStdString();
    if (lastError.text().contains("syntax error"))
    {
        // Ad hoc: "syntax error" in `analytics::db::Filter::freeText` is reported by QSqlQuery
        // as QSqlError::ConnectionError, which is http 500, but 400 is desired for invalid input.
        res.code = DBResultCode::statementError;
    }

    return res;
}


/**
* Returns more detailed result code if appropriate. Otherwise returns initial one.
*/
//DBResult detailResultCode(AbstractDbConnection* const connection, DBResult result) const;

//DBResult BaseExecutor::detailResultCode(
//    AbstractDbConnection* const connection,
//    DBResult result) const
//{
//    if (result != DBResultCode::ioError)
//        return result;
//    switch (connection->lastError().type())
//    {
//        case QSqlError::StatementError:
//            return DBResultCode::statementError;
//        case QSqlError::ConnectionError:
//            return DBResult::connectionError;
//        default:
//            return result;
//    }
//}

} // namespace nx::sql
