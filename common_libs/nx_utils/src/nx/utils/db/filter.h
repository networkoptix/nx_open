#pragma once

#include <array>
#include <cstdint>

#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>

#include "query.h"

namespace nx {
namespace utils {
namespace db {

struct SqlFilterField
{
    const char* name;
    const char* placeHolderName;
    QVariant value;
};

using InnerJoinFilterFields = std::vector<SqlFilterField>;

/**
 * @return fieldName1=placeHolderName1#separator...fieldNameN=placeHolderNameN
 */
NX_UTILS_API QString joinFields(const InnerJoinFilterFields& fields, const QString& separator);
NX_UTILS_API void bindFields(QSqlQuery* const query, const InnerJoinFilterFields& fields);
NX_UTILS_API void bindFields(SqlQuery* const query, const InnerJoinFilterFields& fields);

NX_UTILS_API QString generateWhereClauseExpression(const InnerJoinFilterFields& filter);

} // namespace db
} // namespace utils
} // namespace nx
