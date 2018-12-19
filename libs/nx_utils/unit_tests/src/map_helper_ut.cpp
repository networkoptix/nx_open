#include <gtest/gtest.h>

#include <map>
#include <string>

#include <nx/utils/data_structures/map_helper.h>

namespace nx::utils::data_structures::test {

static const std::map<std::string, int> kInternalMap = {
    {"value", 5},
    {"otherValue", 7}
};

class MapHelperTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        MapHelper::set(&m_nestedMap, "setting", "some", "value", 5);
        MapHelper::set(&m_nestedMap, "setting", "some", "otherValue", 7);

        MapHelper::set(&m_flatMap, "flatMapKey0", 1);
        MapHelper::set(&m_flatMap, "flatMapKey1", 2);
    }

protected:
    MapHelper::NestedMap<std::map, std::string, std::string, std::string, int> m_nestedMap;
    MapHelper::NestedMap<std::map, std::string, int> m_flatMap;
};

TEST_F(MapHelperTest, isMap)
{
    using Map = std::map<std::string, int>;
    Map someMap;
    ASSERT_TRUE(MapHelper::isMap<Map>());
    ASSERT_TRUE(MapHelper::isMap<Map&>());
    ASSERT_TRUE(MapHelper::isMap<Map&&>());

    ASSERT_TRUE(MapHelper::isMap<const Map>());
    ASSERT_TRUE(MapHelper::isMap<const Map&>());
    ASSERT_TRUE(MapHelper::isMap<const Map&&>());

    ASSERT_TRUE(MapHelper::isMap(std::map<std::string, int>()));
    ASSERT_TRUE(MapHelper::isMap(someMap));
}

TEST_F(MapHelperTest, getOrThrow)
{
    ASSERT_EQ(MapHelper::getOrThrow(m_nestedMap, "setting", "some"), kInternalMap);
    ASSERT_EQ(MapHelper::getOrThrow(m_nestedMap, "setting", "some", "value"), 5);
    ASSERT_EQ(MapHelper::getOrThrow(m_nestedMap, "setting", "some", "otherValue"), 7);

    ASSERT_EQ(MapHelper::getOrThrow(m_flatMap, "flatMapKey0"), 1);
    ASSERT_EQ(MapHelper::getOrThrow(m_flatMap, "flatMapKey1"), 2);

    ASSERT_THROW(
        MapHelper::getOrThrow(m_nestedMap, "getting", "some", "value"),
        std::out_of_range);
    ASSERT_THROW(
        MapHelper::getOrThrow(m_nestedMap, "getting", "some"),
        std::out_of_range);
    ASSERT_THROW(
        MapHelper::getOrThrow(m_flatMap, "flatMapKey2"),
        std::out_of_range);
}

TEST_F(MapHelperTest, getPointer)
{
    ASSERT_EQ(*MapHelper::getPointer(m_nestedMap, "setting", "some"), kInternalMap);
    ASSERT_EQ(*MapHelper::getPointer(m_nestedMap, "setting", "some", "value"), 5);
    ASSERT_EQ(*MapHelper::getPointer(m_nestedMap, "setting", "some", "otherValue"), 7);

    ASSERT_EQ(*MapHelper::getPointer(m_flatMap, "flatMapKey0"), 1);
    ASSERT_EQ(*MapHelper::getPointer(m_flatMap, "flatMapKey1"), 2);

    ASSERT_EQ(MapHelper::getPointer(m_nestedMap, "getting", "some", "value"), nullptr);
    ASSERT_EQ(MapHelper::getPointer(m_nestedMap, "getting", "some"), nullptr);
    ASSERT_EQ(MapHelper::getPointer(m_flatMap, "flatMapKey2"), nullptr);
}

TEST_F(MapHelperTest, getOptional)
{
    ASSERT_EQ(*MapHelper::getOptional(m_nestedMap, "setting", "some"), kInternalMap);
    ASSERT_EQ(*MapHelper::getOptional(m_nestedMap, "setting", "some", "value"), 5);
    ASSERT_EQ(*MapHelper::getOptional(m_nestedMap, "setting", "some", "otherValue"), 7);

    ASSERT_EQ(*MapHelper::getOptional(m_flatMap, "flatMapKey0"), 1);
    ASSERT_EQ(*MapHelper::getOptional(m_flatMap, "flatMapKey1"), 2);

    ASSERT_EQ(MapHelper::getOptional(m_nestedMap, "getting", "some", "value"), std::nullopt);
    ASSERT_EQ(MapHelper::getOptional(m_nestedMap, "getting", "some"), std::nullopt);
    ASSERT_EQ(MapHelper::getOptional(m_flatMap, "flatMapKey2"), std::nullopt);
}

TEST_F(MapHelperTest, contains)
{
    ASSERT_TRUE(MapHelper::contains(m_nestedMap, "setting", "some", "value"));
    ASSERT_TRUE(MapHelper::contains(m_nestedMap, "setting", "some", "otherValue"));
    ASSERT_TRUE(MapHelper::contains(m_nestedMap, "setting", "some"));
    ASSERT_TRUE(MapHelper::contains(m_flatMap, "flatMapKey0"));
    ASSERT_TRUE(MapHelper::contains(m_flatMap, "flatMapKey1"));

    ASSERT_FALSE(MapHelper::contains(m_nestedMap, "setting", "someOtherStuff"));
    ASSERT_FALSE(MapHelper::contains(m_nestedMap, "setting", "some", "otherValue2"));
    ASSERT_FALSE(MapHelper::contains(m_flatMap, "flatMapKey2"));
}

TEST_F(MapHelperTest, mergeNested)
{
    MapHelper::NestedMap<std::map, std::string, std::string, int> first;
    MapHelper::NestedMap<std::map, std::string, std::string, int> second;

    MapHelper::set(&first, "hello0", "section0", 1);
    MapHelper::set(&first, "hello0", "section1", 2);
    MapHelper::set(&first, "hello1", "section1", 3);
    MapHelper::set(&first, "hello2", "section0", 4);

    MapHelper::set(&second, "hello0", "section0", 5);
    MapHelper::set(&second, "hello0", "section1", 6);
    MapHelper::set(&second, "hello1", "section1", 7);
    MapHelper::set(&second, "hello3", "section4", 8);

    MapHelper::NestedMap<std::map, std::string, std::string, int> expectedResult;
    MapHelper::set(&expectedResult, "hello0", "section0", 5);
    MapHelper::set(&expectedResult, "hello0", "section1", 6);
    MapHelper::set(&expectedResult, "hello1", "section1", 7);
    MapHelper::set(&expectedResult, "hello3", "section4", 8);
    MapHelper::set(&expectedResult, "hello2", "section0", 4);

    const auto result = MapHelper::merge(first, second);
    ASSERT_EQ(result, expectedResult);
}

TEST_F(MapHelperTest, merge)
{
    MapHelper::NestedMap<std::map, std::string, int> first = {
        {"first", 1},
        {"second", 2}
    };

    MapHelper::NestedMap<std::map, std::string, int> second = {
        {"first", 1},
        {"third", 3}
    };

    MapHelper::NestedMap<std::map, std::string, int> expectedResult = {
        {"first", 1},
        {"second", 2},
        {"third", 3}
    };

    const auto result = MapHelper::merge(first, second);
    ASSERT_EQ(result, expectedResult);
}

TEST_F(MapHelperTest, mergeInPlace)
{
    MapHelper::NestedMap<std::map, std::string, int> first = {
        {"first", 1},
        {"second", 2}
    };

    MapHelper::NestedMap<std::map, std::string, int> second = {
        {"first", 1},
        {"third", 3}
    };

    MapHelper::NestedMap<std::map, std::string, int> expectedResult = {
        {"first", 1},
        {"second", 2},
        {"third", 3}
    };

    MapHelper::merge(&first, second);
    ASSERT_EQ(first, expectedResult);
}

TEST_F(MapHelperTest, erase)
{
    decltype(m_nestedMap) expectedResult;
    MapHelper::set(&expectedResult, "setting", "some", "otherValue", 7);

    MapHelper::erase(&m_nestedMap, "setting", "some", "value");
    MapHelper::erase(&m_nestedMap, "setting", "some", "nonexistentValue");

    ASSERT_EQ(m_nestedMap, expectedResult);
}

TEST_F(MapHelperTest, keys)
{
    const std::set<std::tuple<std::string, std::string, std::string>> kNestedResult = {
        {"setting", "some", "value"},
        {"setting", "some", "otherValue"}
    };

    const std::set<std::tuple<std::string>> kFlatResult = {
        {"flatMapKey0"},
        {"flatMapKey1"}
    };

    ASSERT_EQ(kNestedResult, MapHelper::keys(m_nestedMap));
    ASSERT_EQ(kFlatResult, MapHelper::keys(m_flatMap));
}

TEST_F(MapHelperTest, forEach)
{
    decltype(m_nestedMap) result;
    auto func =
        [&result](const auto& keys, const auto& value)
        {
            std::apply(
                NX_WRAP_FUNC_TO_LAMBDA(MapHelper::set),
                MapHelper::flatTuple(&result, keys, value));
        };

    MapHelper::forEach(m_nestedMap, func);
    ASSERT_EQ(result, m_nestedMap);
}

} // namespace nx::utils::data_structures::test
