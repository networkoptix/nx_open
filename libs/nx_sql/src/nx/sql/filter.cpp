#include "filter.h"

#include <nx/utils/log/log_message.h>

namespace nx::sql {

SqlFilterField::SqlFilterField(
    const char* name,
    const char* placeHolderName,
    QVariant value,
    const char* comparisonOperator)
    :
    m_name(name),
    m_placeHolderName(placeHolderName),
    m_value(std::move(value)),
    m_comparisonOperator(comparisonOperator)
{
}

std::string SqlFilterField::toString() const
{
    return lm("%1%2%3").args(
        m_name,
        m_comparisonOperator,
        m_placeHolderName).toStdString();
}

void SqlFilterField::bindFields(QSqlQuery* query) const
{
    query->bindValue(QLatin1String(m_placeHolderName), m_value);
}

const char* SqlFilterField::name() const
{
    return m_name;
}

const char* SqlFilterField::placeHolderName() const
{
    return m_placeHolderName;
}

//-------------------------------------------------------------------------------------------------

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

SqlFilterFieldLessOrEqual::SqlFilterFieldLessOrEqual(
    const char* name,
    const char* placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "<=")
{
}

//-------------------------------------------------------------------------------------------------

SqlFilterFieldAnyOf::SqlFilterFieldAnyOf(
    const char* name,
    const char* placeHolderName)
    :
    m_name(name),
    m_placeHolderName(placeHolderName)
{
}

std::string SqlFilterFieldAnyOf::toString() const
{
    std::string result = std::string(m_name) + " IN (";
    for (int i = 0; i < (int) m_values.size(); ++i)
    {
        if (i > 0)
            result += ",";
        result += m_placeHolderName;
        result += std::to_string(i);
    }
    result += ")";
    return result;
}

void SqlFilterFieldAnyOf::bindFields(QSqlQuery* query) const
{
    for (int i = 0; i < (int) m_values.size(); ++i)
        query->bindValue((m_placeHolderName + std::to_string(i)).c_str(), m_values[i]);
}

void SqlFilterFieldAnyOf::addValue(const QVariant& value)
{
    m_values.push_back(value);
}

//-------------------------------------------------------------------------------------------------

void Filter::addCondition(std::unique_ptr<AbstractFilterCondition> condition)
{
    m_conditions.push_back(std::move(condition));
}

std::string Filter::toString() const
{
    std::string result;
    for (const auto& filterField: m_conditions)
    {
        if (!result.empty())
            result += " AND ";
        result += filterField->toString();
    }
    return result;
}

void Filter::bindFields(QSqlQuery* query) const
{
    for (const auto& filterField: m_conditions)
        filterField->bindFields(query);
}

void Filter::bindFields(SqlQuery* query) const
{
    for (const auto& filterField: m_conditions)
        filterField->bindFields(&query->impl());
}

//-------------------------------------------------------------------------------------------------

std::string joinFields(
    const InnerJoinFilterFields& fields,
    const QString& separator)
{
    QStringList result;
    for (const auto& field: fields)
    {
        result.push_back(QLatin1String(field.name()) + '=' + 
            QLatin1String(field.placeHolderName()));
    }
    return result.join(separator).toStdString();
}

void bindFields(QSqlQuery* query, const InnerJoinFilterFields& fields)
{
    for (const auto& fieldFilter: fields)
        fieldFilter.bindFields(query);
}

void bindFields(SqlQuery* query, const InnerJoinFilterFields& fields)
{
    for (const auto& fieldFilter: fields)
        fieldFilter.bindFields(&query->impl());
}

std::string generateWhereClauseExpression(const InnerJoinFilterFields& filter)
{
    std::string result;
    for (const auto& filterField: filter)
    {
        if (!result.empty())
            result += " AND ";
        result += filterField.toString();
    }
    return result;
}

} // namespace nx::sql
