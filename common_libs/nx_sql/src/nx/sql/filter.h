#pragma once

#include <array>
#include <cstdint>

#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>

#include "query.h"

namespace nx::sql {

struct NX_SQL_API SqlFilterField
{
    const char* name;
    const char* placeHolderName;
    QVariant value;
    const char* comparisonOperator;

    SqlFilterField(
        const char* name,
        const char* placeHolderName,
        QVariant value,
        const char* comparisonOperator);
};

struct NX_SQL_API SqlFilterFieldEqual:
    SqlFilterField
{
    SqlFilterFieldEqual(
        const char* name,
        const char* placeHolderName,
        QVariant value);
};

struct NX_SQL_API SqlFilterFieldGreaterOrEqual:
    SqlFilterField
{
    SqlFilterFieldGreaterOrEqual(
        const char* name,
        const char* placeHolderName,
        QVariant value);
};

struct NX_SQL_API SqlFilterFieldLess:
    SqlFilterField
{
    SqlFilterFieldLess(
        const char* name,
        const char* placeHolderName,
        QVariant value);
};

struct NX_SQL_API SqlFilterFieldLessOrEqual:
    SqlFilterField
{
    SqlFilterFieldLessOrEqual(
        const char* name,
        const char* placeHolderName,
        QVariant value);
};

using InnerJoinFilterFields = std::vector<SqlFilterField>;

/**
 * @return fieldName1=placeHolderName1#separator...fieldNameN=placeHolderNameN
 */
NX_SQL_API QString joinFields(const InnerJoinFilterFields& fields, const QString& separator);
NX_SQL_API void bindFields(QSqlQuery* const query, const InnerJoinFilterFields& fields);
NX_SQL_API void bindFields(SqlQuery* const query, const InnerJoinFilterFields& fields);

NX_SQL_API QString generateWhereClauseExpression(const InnerJoinFilterFields& filter);

} // namespace nx::sql
