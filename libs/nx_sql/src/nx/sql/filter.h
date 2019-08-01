#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>

#include "query.h"

namespace nx::sql {

class NX_SQL_API AbstractFilterCondition
{
public:
    virtual ~AbstractFilterCondition() = default;

    virtual std::string toString() const = 0;
    virtual void bindFields(QSqlQuery* query) const = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API SqlFilterField:
    public AbstractFilterCondition
{
public:
    SqlFilterField(
        const std::string& name,
        const std::string& placeHolderName,
        QVariant value,
        const std::string& comparisonOperator);

    virtual std::string toString() const override;
    virtual void bindFields(QSqlQuery* query) const override;

    const std::string& name() const;
    const std::string& placeHolderName() const;

private:
    std::string m_name;
    std::string m_placeHolderName;
    QVariant m_value;
    std::string m_comparisonOperator;
};

class NX_SQL_API SqlFilterFieldEqual:
    public SqlFilterField
{
public:
    SqlFilterFieldEqual(
        const std::string& name,
        const std::string& placeHolderName,
        QVariant value);
};

class NX_SQL_API SqlFilterFieldGreaterOrEqual:
    public SqlFilterField
{
public:
    SqlFilterFieldGreaterOrEqual(
        const std::string& name,
        const std::string& placeHolderName,
        QVariant value);
};

class NX_SQL_API SqlFilterFieldLess:
    public SqlFilterField
{
public:
    SqlFilterFieldLess(
        const std::string& name,
        const std::string& placeHolderName,
        QVariant value);
};

class NX_SQL_API SqlFilterFieldLessOrEqual:
    public SqlFilterField
{
public:
    SqlFilterFieldLessOrEqual(
        const std::string& name,
        const std::string& placeHolderName,
        QVariant value);
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API SqlFilterFieldAnyOf:
    public AbstractFilterCondition
{
public:
    SqlFilterFieldAnyOf(
        const std::string& name,
        const std::string& placeHolderName);

    template<typename... Values>
    SqlFilterFieldAnyOf(
        const std::string& name,
        const std::string& placeHolderName,
        Values... values)
        :
        SqlFilterFieldAnyOf(name, placeHolderName)
    {
        m_values = {QVariant(values)...};
    }

    template<typename Value>
    SqlFilterFieldAnyOf(
        const std::string& name,
        const std::string& placeHolderName,
        const std::vector<Value>& values)
        :
        SqlFilterFieldAnyOf(name, placeHolderName)
    {
        for (const auto& value: values)
            m_values.push_back(QVariant(value));
    }

    virtual std::string toString() const override;
    virtual void bindFields(QSqlQuery* query) const override;

    void addValue(const QVariant& value);

private:
    std::string m_name;
    std::string m_placeHolderName;
    std::vector<QVariant> m_values;
};

//-------------------------------------------------------------------------------------------------

using InnerJoinFilterFields = std::vector<SqlFilterField>;

class NX_SQL_API Filter
{
public:
    Filter() = default;
    Filter(const Filter&) = delete;
    Filter& operator=(const Filter&) = delete;
    Filter(Filter&&) = default;
    Filter& operator=(Filter&&) = default;

    void addCondition(std::unique_ptr<AbstractFilterCondition> condition);

    std::string toString() const;

    void bindFields(QSqlQuery* query) const;
    void bindFields(AbstractSqlQuery* query) const;

private:
    std::vector<std::unique_ptr<AbstractFilterCondition>> m_conditions;
};

/**
 * @return fieldName1=placeHolderName1#separator...fieldNameN=placeHolderNameN
 */
NX_SQL_API std::string joinFields(
    const InnerJoinFilterFields& fields,
    const std::string& separator);

NX_SQL_API void bindFields(QSqlQuery* query, const InnerJoinFilterFields& fields);

NX_SQL_API void bindFields(SqlQuery* query, const InnerJoinFilterFields& fields);

NX_SQL_API std::string generateWhereClauseExpression(const InnerJoinFilterFields& filter);

} // namespace nx::sql
