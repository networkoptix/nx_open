#include <gtest/gtest.h>

#include <nx/analytics/db/attributes_dao.h>

namespace nx::analytics::db::test {

class AttributesDao:
    public ::testing::Test
{
protected:
    QString prepareFtsFilter(const QString& text)
    {
        return db::AttributesDao::convertTextFilterToSqliteFtsExpression(text);
    }
};

TEST_F(AttributesDao, prepare_sqlite_fts_expression)
{
    const QString kParamNamePrefix = "000";

    ASSERT_EQ("val1 val2", prepareFtsFilter("val1 val2"));
    ASSERT_EQ(kParamNamePrefix + "name1 NEAR/0 val1", prepareFtsFilter("name1:val1"));
    ASSERT_EQ(kParamNamePrefix + "name1 NEAR/0 val1", prepareFtsFilter("name1: val1"));
    ASSERT_EQ(kParamNamePrefix + "name1 NEAR/0 val1", prepareFtsFilter("name1 :val1"));
    ASSERT_EQ(kParamNamePrefix + "name1", prepareFtsFilter(kParamNamePrefix + "name1"));
}

} // namespace nx::analytics::db::test
