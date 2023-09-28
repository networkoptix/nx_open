// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QtCore/QList>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/utils/string.h>

namespace test {

class QtContainersNxReflectSupport:
    public ::testing::Test
{
protected:
    template<typename T>
    void assertWorks(const T& v, const std::string& expectedJson)
    {
        const auto serialized = nx::reflect::json::serialize(v);
        ASSERT_EQ(expectedJson, serialized);

        const auto [deserialized, result] = nx::reflect::json::deserialize<T>(serialized);
        ASSERT_TRUE(result);
        ASSERT_EQ(v, deserialized);
    }
};

namespace {

template<typename T>
struct Foo
{
    T v;

    bool operator==(const Foo& other) const { return v == other.v; }
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(Foo, (v))

} // namespace

//-------------------------------------------------------------------------------------------------

namespace {
using FooVector = Foo<QVector<std::string>>;
} // namespace

TEST_F(QtContainersNxReflectSupport, QVector)
{
    FooVector foo{{"1", "2", "3"}};
    assertWorks(foo, R"({"v":["1","2","3"]})");
    assertWorks(foo.v, R"(["1","2","3"])");

    assertWorks(FooVector{}, "{\"v\":[]}");
    assertWorks(FooVector{}.v, "[]");
}

//-------------------------------------------------------------------------------------------------

namespace {
using FooList = Foo<QList<std::string>>;
} // namespace

TEST_F(QtContainersNxReflectSupport, QList)
{
    FooList foo{{"1", "2", "3"}};
    assertWorks(foo, R"({"v":["1","2","3"]})");
    assertWorks(foo.v, R"(["1","2","3"])");

    assertWorks(FooList{}, "{\"v\":[]}");
    assertWorks(FooList{}.v, "[]");
}

//-------------------------------------------------------------------------------------------------

namespace {
using FooSet = Foo<QSet<QString>>;
} // namespace

TEST_F(QtContainersNxReflectSupport, QSet)
{
    FooSet foo{{"3", "1", "2"}};

    std::string expected;
    // NOTE: QSet is actually a hash-table so the order of its elements is undefined.
    expected += "{\"v\":";

    std::string setStr;
    setStr += "[";
    for (const auto& v: foo.v)
        setStr += "\"" + v.toStdString() + "\",";
    if (!foo.v.empty())
        setStr.pop_back();
    setStr += "]";

    expected += setStr;
    expected += "}";

    // Elements are expected to be sorted in the resulting json.
    assertWorks(foo, expected);
    assertWorks(foo.v, setStr);
}

//-------------------------------------------------------------------------------------------------

namespace {
using FooMap = Foo<QMap<QString, QList<int>>>;
} // namespace

TEST_F(QtContainersNxReflectSupport, QMap)
{
    FooMap foo{ {{"1", {11,12,13}}, {"2", {21,22,23}}, {"3",{31,32,33}}} };

    assertWorks(foo, R"({"v":{"1":[11,12,13],"2":[21,22,23],"3":[31,32,33]}})");
    assertWorks(foo.v, R"({"1":[11,12,13],"2":[21,22,23],"3":[31,32,33]})");
}

//-------------------------------------------------------------------------------------------------

namespace {
using FooHash = Foo<QHash<QString, QString>>;
} // namespace

TEST_F(QtContainersNxReflectSupport, QHash)
{
    FooHash foo{{{"1", "v1"}, {"2", "v2"}, {"3", "v3"}}};

    // NOTE: QHash is a hash-table so the order of its elements is undefined.
    std::string hashStr;
    hashStr += "{";
    for (auto it = foo.v.begin(); it != foo.v.end(); ++it)
    {
        hashStr += "\"" + it.key().toStdString() + "\":";
        hashStr += "\"" + it.value().toStdString() + "\"";
        hashStr += ",";
    }
    if (!foo.v.empty())
        hashStr.pop_back();
    hashStr += "}";

    assertWorks(foo.v, hashStr);
    assertWorks(foo, "{\"v\":" + hashStr + "}");
}

} // namespace test
