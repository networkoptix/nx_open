// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/array_orderer.h>

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

TEST(ArrayOrderer, Simple)
{
    std::vector<Model> list{{"name", 2}, {"name2", 1}, {"name1", 1}};
    std::vector<ArrayOrder> fields{{"number"}, {"name"}};
    order(&list, {.fields = fields});
    std::vector<Model> expectedList{{"name1", 1}, {"name2", 1}, {"name", 2}};
    ASSERT_EQ(list, expectedList);
}

TEST(ArrayOrderer, Reverse)
{
    std::vector<Model> list{{"name2", 1}, {"name1", 1}, {"name", 2}};
    std::vector<ArrayOrder> fields{{"number", /*reverse*/ true}, {"name"}};
    order(&list, {.fields = fields});
    std::vector<Model> expectedList{{"name", 2}, {"name1", 1}, {"name2", 1}};
    ASSERT_EQ(list, expectedList);
}

TEST(ArrayOrderer, Nested)
{
    std::vector<Model> list{
        {.nested = {"name2", 1}}, {.nested = {"name1", 1}}, {.nested = {"name", 2}}};
    std::vector<ArrayOrder> nestedFields{{"number", /*reverse*/ true}, {"name"}};
    std::vector<ArrayOrder> fields{{.name = "nested", .fields = nestedFields}};
    order(&list, {.fields = fields});
    std::vector<Model> expectedList{
        {.nested = {"name", 2}}, {.nested = {"name1", 1}}, {.nested = {"name2", 1}}};
    ASSERT_EQ(list, expectedList);
}

TEST(ArrayOrderer, NestedList)
{
    std::vector<Model2> list{
        {"name2", 1, std::vector<Nested>{{"name2", 1}, {"name1", 1}, {"name", 2}}}, {"name1", 1}};
    std::vector<ArrayOrder> nestedFields{{"number", /*reverse*/ true}, {"name"}};
    std::vector<ArrayOrder> fields{{"name2"}, {.name = "nestedList", .fields = nestedFields}};
    order(&list, {.fields = fields});
    std::vector<Model2> expectedList{
        {"name1", 1}, {"name2", 1, std::vector<Nested>{{"name", 2}, {"name1", 1}, {"name2", 1}}}};
    ASSERT_EQ(list, expectedList);
}

TEST(ArrayOrderer, Variant)
{
    std::vector<std::variant<Model, Model2>> list{
        Model2{"name", 2},
        Model2{"name2", 1},
        Model2{"name1", 1, {}, 2},
        Model2{"name1", 1, {}, 1},
        Model2{"name1", 1},
    };
    std::vector<ArrayOrder> fields{
        {"number2"},
        {"name2"},
        {"variant"},
    };
    order(&list, {.fields = {{.variantIndex = 1, .fields = fields}}});
    std::vector<std::variant<Model, Model2>> expectedList{
        Model2{"name1", 1, {}, 1},
        Model2{"name1", 1, {}, 2},
        Model2{"name1", 1},
        Model2{"name2", 1},
        Model2{"name", 2},
    };
    ASSERT_EQ(list, expectedList);
}

TEST(ArrayOrderer, InMap)
{
    std::map<int, std::vector<Model>> map{{0, {{"name", 2}, {"name2", 1}, {"name1", 1}}}};
    std::vector<ArrayOrder> fields{{"number"}, {"name"}};
    order(&map, {.fields = fields});
    std::map<int, std::vector<Model>> expectedMap{{0, {{"name1", 1}, {"name2", 1}, {"name", 2}}}};
    ASSERT_EQ(map, expectedMap);
}

} // namespace nx::reflect::test
