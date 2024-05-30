// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
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

class NX_SQL_API SqlFilterFieldNotEqual:
    public SqlFilterField
{
public:
    SqlFilterFieldNotEqual(
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

/**
 * Helper class for generating SQL WHERE conditions like `col IN (val1, val2, val3, etc...)`.
 *
 * Usage example:
 * <pre><code>
 * SqlFilterFieldAnyOf condition("table_column_name", ":placeHolder");
 * condition.addValue("va1");
 * condition.addValue("va2");
 * ASSERT_EQ("table_column_name IN (:placeHolder0, :placeHolder1)", condition.toString());
 *
 * // The following calls binds "val1" -> :placeHolder0, "val2" -> :placeHolder1, etc...
 * condition.bindFields(someQuery);
 * </code></pre>
 */
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
        m_values = {toVariant(values)...};
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
            m_values.push_back(toVariant(value));
    }

    virtual std::string toString() const override;
    virtual void bindFields(QSqlQuery* query) const override;

    void addValue(const QVariant& value);
    void addValue(const std::string& value);

private:
    std::string m_name;
    std::string m_placeHolderName;
    std::vector<QVariant> m_values;

    template<typename T>
    QVariant toVariant(T&& value)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
            return QVariant(QString::fromStdString(value));
        else
            return QVariant(std::forward<T>(value));
    }
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

    bool empty() const;

    /**
     * @return Filter expression that can be used in a WHERE clause.
     * NOTE: "WHERE " is not added. Example: "field1=:field1 and field2=:field2"
     */
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

NX_SQL_API void bindFields(AbstractSqlQuery* query, const InnerJoinFilterFields& fields);

NX_SQL_API std::string generateWhereClauseExpression(const InnerJoinFilterFields& filter);

//-------------------------------------------------------------------------------------------------

/**
 * Represents LIMIT <int> [OFFSET <int>].
 */
struct NX_SQL_API Range
{
    int limit = 0;
    std::optional<int> offset;

    std::string toString() const;
    void bind(AbstractSqlQuery* query) const;
};

} // namespace nx::sql
