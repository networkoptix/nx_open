// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

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
    virtual void prepare(const std::string_view& query) = 0;

    template<typename... Params>
    void prepareWithValues(const std::string_view& query, Params&&... params);

    virtual void addBindValue(const QVariant& value) noexcept = 0;
    virtual void addBindValue(const std::string_view& value) noexcept = 0;
    virtual void bindValue(const QString& placeholder, const QVariant& value) noexcept = 0;
    virtual void bindValue(int pos, const QVariant& value) noexcept = 0;
    virtual void bindValue(const std::string_view& placeholder, const std::string_view& value) noexcept = 0;
    virtual void bindValue(int pos, const std::string_view& value) noexcept = 0;

    virtual void exec() = 0;
    virtual void exec(const std::string_view& query) = 0;

    virtual bool next() = 0;

    virtual QVariant value(int index) const = 0;
    virtual QVariant value(const QString& name) const = 0;

    template<typename T> T value(int index) const;
    template<typename T> T value(const char* name) const;

    virtual QSqlRecord record() = 0;
    virtual QVariant lastInsertId() const = 0;
    virtual int numRowsAffected() const = 0;

    // TODO: #akolesnikov Remove these methods.
    // For now they are required for compatibility with QnSql::*.
    virtual QSqlQuery& impl() = 0;
    virtual const QSqlQuery& impl() const = 0;
};

template<typename T> T AbstractSqlQuery::value(int index) const
{
    return value(index).value<T>();
}

template<> inline std::string AbstractSqlQuery::value<std::string>(int index) const
{
    return value(index).toString().toStdString();
}

template<typename T> T AbstractSqlQuery::value(const char* name) const
{
    return value(name).value<T>();
}

template<> inline std::string AbstractSqlQuery::value<std::string>(const char* name) const
{
    return value(name).toString().toStdString();
}

template<typename... Params>
inline void AbstractSqlQuery::prepareWithValues(const std::string_view& query, Params&&... params)
{
    prepare(query);
    (addBindValue(std::forward<Params>(params)), ...);
}
//-------------------------------------------------------------------------------------------------

class NX_SQL_API SqlQuery:
    public AbstractSqlQuery
{
public:
    SqlQuery(QSqlDatabase connection);
    SqlQuery(AbstractDbConnection* connection);

    virtual void setForwardOnly(bool val) override;
    virtual void prepare(const std::string_view& query) override;

    virtual void addBindValue(const QVariant& value) noexcept override;
    virtual void addBindValue(const std::string_view& value) noexcept override;
    virtual void bindValue(const QString& placeholder, const QVariant& value) noexcept override;
    virtual void bindValue(int pos, const QVariant& value) noexcept override;
    virtual void bindValue(const std::string_view& placeholder, const std::string_view& value) noexcept override;
    virtual void bindValue(int pos, const std::string_view& value) noexcept override;

    virtual void exec() override;
    virtual void exec(const std::string_view& query) override;

    virtual bool next() override;
    virtual QVariant value(int index) const override;
    virtual QVariant value(const QString& name) const override;
    virtual QSqlRecord record() override;
    virtual QVariant lastInsertId() const override;
    virtual int numRowsAffected() const override;

    virtual QSqlQuery& impl() override;
    virtual const QSqlQuery& impl() const override;

    static void exec(QSqlDatabase connection, const QByteArray& queryText);
    static void exec(AbstractDbConnection* connection, const QByteArray& queryText);

private:
    QSqlQuery m_sqlQuery;

    std::optional<QString> m_unpreparedQuery;

private:
    void exec(const std::optional<std::string_view>& query);

    bool shouldPrepare(const std::string_view& query);

    DBResult getLastError();
};

} // namespace nx::sql
