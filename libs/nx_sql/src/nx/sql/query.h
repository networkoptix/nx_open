#pragma once

#include <QtCore/QVariant>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlQuery>

#include "types.h"

namespace nx::sql {

class AbstractDbConnection;

/**
 * Follows same conventions as QSqlQuery except error reporting:
 * methods of this class throw nx::sql::Exception on error.
 */
class NX_SQL_API AbstractSqlQuery
{
public:
    virtual ~AbstractSqlQuery() = default;

    virtual void setForwardOnly(bool val) = 0;
    virtual void prepare(const QString& query) = 0;

    virtual void addBindValue(const QVariant& value) noexcept = 0;
    virtual void bindValue(const QString& placeholder, const QVariant& value) noexcept = 0;
    virtual void bindValue(int pos, const QVariant& value) noexcept = 0;
    virtual void bindValue(const std::string& placeholder, const std::string& value) noexcept = 0;
    virtual void bindValue(int pos, const std::string& value) noexcept = 0;

    virtual void exec() = 0;

    virtual bool next() = 0;
    virtual QVariant value(int index) const = 0;
    virtual QVariant value(const QString& name) const = 0;
    virtual QSqlRecord record() = 0;
    virtual QVariant lastInsertId() const = 0;

    // TODO: #ak Remove these methods.
    // For now they are required for compatibility with QnSql::*.
    virtual QSqlQuery& impl() = 0;
    virtual const QSqlQuery& impl() const = 0;
};

/**
 * Follows same conventions as QSqlQuery except error reporting:
 * methods of this class throw nx::sql::Exception on error.
 */
class NX_SQL_API SqlQuery:
    public AbstractSqlQuery
{
public:
    SqlQuery(QSqlDatabase connection);
    SqlQuery(AbstractDbConnection* connection);

    virtual void setForwardOnly(bool val) override;
    virtual void prepare(const QString& query) override;

    virtual void addBindValue(const QVariant& value) noexcept override;
    virtual void bindValue(const QString& placeholder, const QVariant& value) noexcept override;
    virtual void bindValue(int pos, const QVariant& value) noexcept override;
    virtual void bindValue(const std::string& placeholder, const std::string& value) noexcept override;
    virtual void bindValue(int pos, const std::string& value) noexcept override;

    virtual void exec() override;

    virtual bool next() override;
    virtual QVariant value(int index) const override;
    virtual QVariant value(const QString& name) const override;
    virtual QSqlRecord record() override;
    virtual QVariant lastInsertId() const override;

    virtual QSqlQuery& impl() override;
    virtual const QSqlQuery& impl() const override;

    static void exec(QSqlDatabase connection, const QByteArray& queryText);
    static void exec(AbstractDbConnection* connection, const QByteArray& queryText);

private:
    QSqlQuery m_sqlQuery;

    DBResult getLastErrorCode();
};

} // namespace nx::sql
