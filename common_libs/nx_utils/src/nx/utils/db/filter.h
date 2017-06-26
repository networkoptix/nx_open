#pragma once

#include <array>
#include <cstdint>

#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>

namespace nx {
namespace utils {
namespace db {

struct SqlFilterField
{
    const char* name;
    const char* placeHolderName;
    QVariant value;
};

typedef std::vector<SqlFilterField> InnerJoinFilterFields;

/**
 * @return fieldName1=placeHolderName1#separator...fieldNameN=placeHolderNameN
 */
QString joinFields(const InnerJoinFilterFields& fields, const QString& separator);
void bindFields(QSqlQuery* const query, const InnerJoinFilterFields& fields);

} // namespace db
} // namespace utils
} // namespace nx
