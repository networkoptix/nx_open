#include "filter.h"

#include <nx/utils/log/log_message.h>

namespace nx {
namespace utils {
namespace db {

SqlFilterField::SqlFilterField(
    const char* name,
    const char* placeHolderName,
    QVariant value,
    const char* comparisonOperator)
    :
    name(name),
    placeHolderName(placeHolderName),
    value(std::move(value)),
    comparisonOperator(comparisonOperator)
{
}

SqlFilterFieldEqual::SqlFilterFieldEqual(
    const char* name,
    const char* placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "=")
{
}

SqlFilterFieldGreaterOrEqual::SqlFilterFieldGreaterOrEqual(
    const char* name,
    const char* placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), ">=")
{
}

SqlFilterFieldLess::SqlFilterFieldLess(
    const char* name,
    const char* placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "<")
{
}

//-------------------------------------------------------------------------------------------------

QString joinFields(
    const InnerJoinFilterFields& fields,
    const QString& separator)
{
    QString result;
    for (const auto& field : fields)
        result += lit("%1%2=%3")
            .arg(result.isEmpty() ? QString() : separator)
            .arg(QLatin1String(field.name))
            .arg(QLatin1String(field.placeHolderName));
    return result;
}

void bindFields(QSqlQuery* const query, const InnerJoinFilterFields& fields)
{
    for (const auto& field: fields)
        query->bindValue(QLatin1String(field.placeHolderName), field.value);
}

void bindFields(SqlQuery* const query, const InnerJoinFilterFields& fields)
{
    for (const auto& field: fields)
        query->bindValue(QLatin1String(field.placeHolderName), field.value);
}

QString generateWhereClauseExpression(const InnerJoinFilterFields& filter)
{
    QString result;
    for (const auto& filterField: filter)
    {
        if (!result.isEmpty())
            result += " AND ";
        result += lm("%1%2%3").args(
            filterField.name,
            filterField.comparisonOperator,
            filterField.placeHolderName).toQString();
    }
    return result;
}

} // namespace db
} // namespace utils
} // namespace nx
