#include "filter.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <nx/utils/log/log_message.h>

namespace nx::sql {

SqlFilterField::SqlFilterField(
    const std::string& name,
    const std::string& placeHolderName,
    QVariant value,
    const std::string& comparisonOperator)
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
    query->bindValue(m_placeHolderName.c_str(), m_value);
}

const std::string& SqlFilterField::name() const
{
    return m_name;
}

const std::string& SqlFilterField::placeHolderName() const
{
    return m_placeHolderName;
}

//-------------------------------------------------------------------------------------------------

SqlFilterFieldEqual::SqlFilterFieldEqual(
    const std::string& name,
    const std::string& placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "=")
{
}

SqlFilterFieldGreaterOrEqual::SqlFilterFieldGreaterOrEqual(
    const std::string& name,
    const std::string& placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), ">=")
{
}

SqlFilterFieldLess::SqlFilterFieldLess(
    const std::string& name,
    const std::string& placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "<")
{
}

SqlFilterFieldLessOrEqual::SqlFilterFieldLessOrEqual(
    const std::string& name,
    const std::string& placeHolderName,
    QVariant value)
    :
    SqlFilterField(name, placeHolderName, std::move(value), "<=")
{
}

//-------------------------------------------------------------------------------------------------

SqlFilterFieldAnyOf::SqlFilterFieldAnyOf(
    const std::string& name,
    const std::string& placeHolderName)
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

void Filter::bindFields(AbstractSqlQuery* query) const
{
    for (const auto& filterField: m_conditions)
        filterField->bindFields(&query->impl());
}

//-------------------------------------------------------------------------------------------------

std::string joinFields(
    const InnerJoinFilterFields& fields,
    const std::string& separator)
{
    std::vector<std::string> result;
    result.reserve(fields.size());
    for (const auto& field: fields)
        result.push_back(field.name() + '=' + field.placeHolderName());

    return boost::join(
        boost::make_iterator_range(result.begin(), result.end()),
        separator);
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
