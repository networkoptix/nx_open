#pragma once

#include <QtSql/QSqlQuery>

#include "types.h"

namespace nx {
namespace db {

/**
 * Follows same conventions as QSqlQuery except error reporting: 
 * methods of this class throw nx::db::Exception on error.
 */
class SqlQuery
{
public:
    SqlQuery(QSqlDatabase connection);

    void setForwardOnly(bool val);
    void prepare(const QString& query);
    void bindValue(const QString& placeholder, const QVariant& value) noexcept;
    void bindValue(int pos, const QVariant& value) noexcept;
    void exec();

    bool next();
    QVariant value(int index) const;
    QVariant value(const QString& name) const;

    QSqlQuery& impl();
    const QSqlQuery& impl() const;

private:
    QSqlQuery m_sqlQuery;

    DBResult getLastErrorCode();
};

} // namespace db
} // namespace nx
