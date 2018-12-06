#include <gtest/gtest.h>

#include <nx/sql/filter.h>

namespace nx::sql::test {

class Filter:
    public ::testing::Test
{
protected:
    void add_field_equal(
        const char* fieldName,
        const char* placeholder,
        QVariant value)
    {
        m_filter.addCondition(std::make_unique<SqlFilterFieldEqual>(
            fieldName, placeholder, std::move(value)));
    }

    template<typename... Values>
    void add_field_any_of(
        const char* fieldName,
        const char* placeholder,
        Values... values)
    {
        m_filter.addCondition(std::make_unique<SqlFilterFieldAnyOf>(
            fieldName, placeholder, std::move(values)...));
    }

    void assertWhereExpressionEqual(const std::string& expected)
    {
        ASSERT_EQ(expected, m_filter.toString());
    }

private:
    sql::Filter m_filter;
};

TEST_F(Filter, fields_joined_by_and)
{
    add_field_equal("field1", ":field1_placeholder", "value1");
    add_field_equal("field2", ":field2_placeholder", "value2");

    assertWhereExpressionEqual(
        "field1=:field1_placeholder AND field2=:field2_placeholder");
}

TEST_F(Filter, fields_joined_by_or)
{
    add_field_equal("field1", ":field1_placeholder", "value1");
    add_field_any_of("field2", ":field2_placeholder", "value21", "value22", "value23");
    add_field_equal("field3", ":field3_placeholder", "value3");

    assertWhereExpressionEqual(
        "field1=:field1_placeholder AND "
        "field2 IN (:field2_placeholder0,:field2_placeholder1,:field2_placeholder2) AND "
        "field3=:field3_placeholder");
}

} // namespace nx::sql::test
