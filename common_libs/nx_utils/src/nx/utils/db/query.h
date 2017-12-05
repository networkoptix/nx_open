#pragma once

#include <QtCore/QVariant>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlQuery>

#include "types.h"

namespace nx {
namespace utils {
namespace db {

/**
 * Follows same conventions as QSqlQuery except error reporting:
 * methods of this class throw nx::utils::db::Exception on error.
 */
class NX_UTILS_API SqlQuery
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
    QSqlRecord record();

    QSqlQuery& impl();
    const QSqlQuery& impl() const;

    static void exec(QSqlDatabase connection, const QByteArray& queryText);

private:
    QSqlQuery m_sqlQuery;

    DBResult getLastErrorCode();
};

} // namespace db
} // namespace utils
} // namespace nx
