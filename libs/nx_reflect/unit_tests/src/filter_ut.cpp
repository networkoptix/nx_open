// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json/filter.h>

namespace nx::reflect::test {

namespace {

struct Nested
{
    std::string name;
    int number;

    bool operator==(const Nested&) const = default;
};
NX_REFLECTION_INSTRUMENT(Nested, (name)(number))

struct Model
{
    std::string name;
    int number;
    Nested nested;

    bool operator==(const Model&) const = default;
};
NX_REFLECTION_INSTRUMENT(Model, (name)(number)(nested))

struct Model2
{
    std::string name2;
    std::optional<int> number2;
    std::vector<Nested> nestedList;
    std::variant<std::string, int> variant;

    bool operator==(const Model2&) const = default;
};
NX_REFLECTION_INSTRUMENT(Model2, (name2)(number2)(nestedList)(variant))

} // namespace

TEST(Filter, Simple)
{
    std::vector<Model> list{{"name", 2}, {"name2", 1}, {"name1", 1}};
    Filter valueFilter{.fields = {{"number", {"1"}}}};
    json::filter(&list, valueFilter);
    std::vector<Model> expectedList{{"name2", 1}, {"name1", 1}};
    ASSERT_EQ(list, expectedList);
}

TEST(Filter, Nested)
{
    std::vector<Model> list{
        {.nested = {"name", 2}}, {.nested = {"name2", 1}}, {.nested = {"name1", 1}}};
    Filter nestedFilter{.name = "nested", .fields = {{"number", {"1"}}}};
    Filter valueFilter{.fields = {nestedFilter}};
    json::filter(&list, valueFilter);
    std::vector<Model> expectedList{{.nested = {"name2", 1}}, {.nested = {"name1", 1}}};
    ASSERT_EQ(list, expectedList);
}

TEST(Filter, NestedList)
{
    std::vector<Model2> list{
        {"name1", 1},
        {"name2", 1, std::vector<Nested>{
            {"name", 2}, {"name1", 1}, {"name", 2}, {"name2", 1}, {"name", 2}}}
    };
    Filter nestedFilter{.name = "nestedList", .fields = {{"number", {"1"}}}};
    Filter valueFilter{.fields = {nestedFilter}};
    json::filter(&list, valueFilter);
    std::vector<Model2> expectedList{
        {"name2", 1, std::vector<Nested>{{"name1", 1}, {"name2", 1}}}};
    ASSERT_EQ(list, expectedList);
}

TEST(Filter, Variant)
{
    std::vector<std::variant<Model, Model2>> list{
        Model2{"name", 2},
        Model2{"name2", 1},
        Model2{"name1", 1, {}, 2},
        Model2{"name1", 2, {}, 1},
        Model2{"name1", 1},
    };
    Filter valueFilter{.fields = {{"variant", {"1"}}}};
    json::filter(&list, valueFilter);
    std::vector<std::variant<Model, Model2>> expectedList{Model2{"name1", 2, {}, 1}};
    ASSERT_EQ(list, expectedList);
}

TEST(Filter, InMap)
{
    std::map<std::string, std::vector<Model2>> map{
        {"0", {{"name", 2}, {"name2", 1}, {"name1", 1}}},
        {"1", {{"name", 1}, {"name2", 1}, {"name1", 1}}},
        {"2", {{"name", 2}, {"name2", 2}, {"name1", 2}}},
    };
    Filter nestedFilter1{.name = "*", .fields = {{"number2", {"1"}}}};
    Filter nestedFilter2{.name = "1", .fields = {{"name2", {"name2"}}}};
    Filter valueFilter{.fields = {nestedFilter1, nestedFilter2}};
    json::filter(&map, valueFilter);
    std::map<std::string, std::vector<Model2>> expectedMap{
        {"0", {{"name2", 1}, {"name1", 1}}},
        {"1", {{"name2", 1}}},
    };
    ASSERT_EQ(map, expectedMap);
}

} // namespace nx::reflect::test
