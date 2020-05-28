#include <gtest/gtest.h>

#include <nx/analytics/db/attributes_dao.h>

namespace nx::analytics::db::test {

class AttributesDao:
    public ::testing::Test
{
protected:
    QString prepareIndexText(
        const QString& objectTypeName,
        const std::vector<common::metadata::Attribute>& attributes)
    {
        return db::AttributesDao::buildSearchableText(objectTypeName, attributes);
    }

    QString prepareSearchExpression(const QString& text)
    {
        return db::AttributesDao::convertTextFilterToSqliteFtsExpression(text);
    }
};

TEST_F(AttributesDao, prepare_searchable_text)
{
    ASSERT_EQ(
        "typename030 000car02Ebrand02Emodel Aston Martin",
        prepareIndexText("typename0", {{"car.brand.model", "Aston Martin"}}));

    ASSERT_EQ(
        "type name 000Wheel020Size 030030030",
        prepareIndexText("type name", {{"Wheel Size", "000"}}));

    ASSERT_EQ(
        "000car02Ebrand02Emodel Aston Martin 000 000Wheel020Size 030030030",
        prepareIndexText("", {{"car.brand.model", "Aston Martin"}, {"Wheel Size", "000"}}));
}

TEST_F(AttributesDao, prepare_sqlite_fts_expression)
{
    ASSERT_EQ("foo*", prepareSearchExpression("foo"));
    ASSERT_EQ("foo030*", prepareSearchExpression("foo0"));
    ASSERT_EQ("foo* hoo030*", prepareSearchExpression("foo hoo0"));

    ASSERT_EQ(
        "000name1* NEAR/0 val1*",
        prepareSearchExpression("name1:val1"));

    ASSERT_EQ(
        "000name1* NEAR/0 val1*",
        prepareSearchExpression("name1: val1"));

    ASSERT_EQ(
        "000name1* NEAR/0 val1*",
        prepareSearchExpression("name1 :val1"));

    ASSERT_EQ(
        "000name1*",
        prepareSearchExpression("$name1"));
}

TEST_F(AttributesDao, prepare_sqlite_fts_expression_escaping_attribute_name)
{
    ASSERT_EQ(
        "000Wheel020Size* NEAR/0 2030*",
        prepareSearchExpression("\"Wheel Size\": 20"));

    ASSERT_EQ(
        "Wheel* 000Size* NEAR/0 2030*",
        prepareSearchExpression("  Wheel Size: 20"));

    ASSERT_EQ(
        "000car02Ebrand02Emodel* NEAR/0 \"Aston Martin\"",
        prepareSearchExpression("car.brand.model:  \"Aston Martin\""));

    ASSERT_EQ(
        "000car02Ebrand02Emodel* NEAR/0 Aston* red*",
        prepareSearchExpression("car.brand.model:  Aston red  "));

    ASSERT_EQ(
        "000car02Ebrand02Emodel* NEAR/0 \"Aston Martin\" 000car02Ecolor* NEAR/0 red*",
        prepareSearchExpression("car.brand.model:\"Aston Martin\" car.color: red"));
}

} // namespace nx::analytics::db::test
