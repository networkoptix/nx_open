#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>


namespace nx::common::metadata::test {


TEST(metadataAttributes, twoGroups)
{
    Attributes attributes;
    attributes.push_back({"name1","value1"});
    attributes.push_back({"name1","value2"});
    attributes.push_back({"name2","value1"});
    attributes.push_back({ "name1","value3" });

    auto result = groupAttributes(attributes, 2);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(3, result[0].totalValues);
    ASSERT_EQ(2, result[0].values.size());
    ASSERT_EQ(1, result[1].values.size());
}

TEST(metadataAttributes, singleGroups)
{
    Attributes attributes;
    attributes.push_back({ "name1","value1" });
    attributes.push_back({ "name1","value2" });

    auto result = groupAttributes(attributes, 2);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(2, result[0].totalValues);
    ASSERT_EQ(2, result[0].values.size());
}

} // namespace
