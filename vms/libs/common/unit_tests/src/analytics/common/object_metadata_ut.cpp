#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>


namespace nx::common::metadata::test {


TEST(metadataAttributes, twoGroups)
{
    Attributes attributes;
    attributes.push_back({"name1","value1"});
    attributes.push_back({"name1","value2"});
    attributes.push_back({"name2","value1"});
    attributes.push_back({"name1","value3"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1", "value2", "value3"}));
    ASSERT_EQ(result[1].name, "name2");
    ASSERT_EQ(result[1].values, QStringList({"value1"}));
}

TEST(metadataAttributes, oneGroup)
{
    Attributes attributes;
    attributes.push_back({"name1", "value1"});
    attributes.push_back({"name1", "value2"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1", "value2"}));
}

TEST(metadataAttributes, simpleGroups)
{
    Attributes attributes;
    attributes.push_back({"name1", "value1"});
    attributes.push_back({"name2", "value2"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1"}));
    ASSERT_EQ(result[1].name, "name2");
    ASSERT_EQ(result[1].values, QStringList({"value2"}));
}

TEST(metadataAttributes, filterOutHidden)
{
    Attributes attributes;
    attributes.push_back({"nx.sys.testAttribute", "value1"});
    attributes.push_back({"nx.sys.testAttribute", "value2"});

    const auto filtered = groupAttributes(attributes); //< Filters hidden attributes out by default.
    ASSERT_EQ(0, filtered.size());

    const auto unfiltered = groupAttributes(attributes, /*filterOutHidden*/ false);
    ASSERT_EQ(1, unfiltered.size());
    ASSERT_EQ(unfiltered[0].name, "nx.sys.testAttribute");
    ASSERT_EQ(unfiltered[0].values, QStringList({"value1", "value2"}));
}

} // namespace
