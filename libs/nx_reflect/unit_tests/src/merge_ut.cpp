// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/merge.h>

namespace nx::reflect::test {

namespace {

struct InnerData
{
    int number{100};
    std::string name{"name"};

    bool operator==(const InnerData&) const = default;
};
NX_REFLECTION_INSTRUMENT(InnerData, (number) (name))

struct MergeModel
{
    std::string id;
    InnerData inner;
    std::vector<InnerData> list;
    std::map<std::string, InnerData> map;
    std::optional<InnerData> optional;
    std::variant<std::string, InnerData> variant;

    bool operator==(const MergeModel&) const = default;
};
NX_REFLECTION_INSTRUMENT(MergeModel, (id) (inner) (list) (map) (optional) (variant))

} // namespace

TEST(Merge, Fixture)
{
    using Fields = DeserializationResult::Fields;
    using Context = json_detail::DeserializationContext;

    rapidjson::Document document1;
    document1.Parse(R"json(
        {
            "variant":{"name":"name1", "number":1},
            "optional":{"name":"name1", "number":1},
            "map":{"key":{"name":"name1", "number":1}},
            "list":[{"name":"name1"}, {"number":1}],
            "inner":{"name":"name1", "number":1},
            "id":"id1"
        }
    )json");
    if (document1.HasParseError())
        return;

    MergeModel data1;
    const auto result1 =
        json::deserialize(Context{document1, (int) json::DeserializationFlag::fields}, &data1);
    const Fields fields1{{
        {"id", Fields{}},
        {"inner", Fields{{{"number"}, {"name"}}}},
        {"list", Fields{}},
        {"map", Fields{{{"key", Fields{{{"number"}, {"name"}}}}}}},
        {"optional", Fields{{{"number"}, {"name"}}}},
        {"variant", Fields{{{"number"}, {"name"}}}},
    }};
    ASSERT_EQ(result1.fields, fields1);

    rapidjson::Document document2;
    document2.Parse(R"json(
        {
            "variant":{"number":2},
            "optional":{"number":2},
            "map":{"key":{"number":2}},
            "list":[{"number":2}, {"name":"name2"}],
            "inner":{"number":2},
            "id":"id2"
        }
    )json");
    if (document2.HasParseError())
        return;

    MergeModel data2;
    auto result2 =
        json::deserialize(Context{document2, (int) json::DeserializationFlag::fields}, &data2);
    const Fields fields2{{
        {"id", Fields{}},
        {"inner", Fields{{{"number"}}}},
        {"list", Fields{}},
        {"map", Fields{{{"key", Fields{{{"number"}}}}}}},
        {"optional", Fields{{{"number"}}}},
        {"variant", Fields{{{"number"}}}},
    }};
    ASSERT_EQ(result2.fields, fields2);

    MergeModel copy = data2;
    merge(&data1, &copy, std::move(result2.fields));

    // Lists are replaced as is.
    data1.list = data2.list;

    // Names were not provided in JSON.
    data2.inner.name = data1.inner.name;
    data2.map.begin()->second.name = data1.map.begin()->second.name;
    data2.optional->name = data1.optional->name;
    std::get<InnerData>(data2.variant).name = std::get<InnerData>(data1.variant).name;

    ASSERT_EQ(data1, data2);
}

} // namespace nx::reflect::test
