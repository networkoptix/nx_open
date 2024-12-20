// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/utils/json/qjson.h>

namespace test {

struct Foo
{
    int num = 0;
    QJsonValue qJson;

    bool operator==(const Foo& other) const = default;
};

NX_REFLECTION_INSTRUMENT(Foo, (num)(qJson))

class Json: public ::testing::Test
{
public:
    template<class T>
    void thenDeserializedCorrect(
        const nx::reflect::DeserializationResult& result, const T& expected, const T& obtained)
    {
        ASSERT_TRUE(result.success);
        ASSERT_EQ(expected, obtained);
    }

    template<class T>
    void checkSerializationDeserealization(const T& obj, const std::string& strRepresentation)
    {
        auto str = nx::reflect::json::serialize(obj);
        ASSERT_EQ(strRepresentation, str);
        T resultObj;
        auto result = nx::reflect::json::deserialize(str, &resultObj);
        thenDeserializedCorrect(result, obj, resultObj);
    }
};

TEST_F(Json, qjson_object_support)
{
    QString jsonStr = R"({"field1":"aa","field2":12})";
    Foo f{10, QJsonDocument::fromJson(jsonStr.toUtf8()).object()};
    checkSerializationDeserealization(f, R"({"num":10,"qJson":{"field1":"aa","field2":12}})");
}

TEST_F(Json, qjson_array_support)
{
    QString jsonStr = R"([1.0,2.0,3.0,4.0,[1.0,2.0]])";
    Foo f{10, QJsonDocument::fromJson(jsonStr.toUtf8()).array()};
    checkSerializationDeserealization(f, R"({"num":10,"qJson":[1,2,3,4,[1,2]]})");
}

TEST_F(Json, qjson_int_support)
{
    Foo f{12, QJsonValue(12)};
    checkSerializationDeserealization(f, R"({"num":12,"qJson":12})");
}

TEST_F(Json, qjson_double_support)
{
    Foo f{12, QJsonValue(12.34)};
    checkSerializationDeserealization(f, R"({"num":12,"qJson":12.34})");
}

TEST_F(Json, qjson_bool_support)
{
    Foo f{12, QJsonValue(false)};
    checkSerializationDeserealization(f, R"({"num":12,"qJson":false})");
}

TEST_F(Json, qjson_value_support)
{
    QJsonArray arr;
    arr.push_back(42);
    arr.push_back("bar");
    arr.push_back(1.23);

    QJsonObject obj;
    obj["num"] = 3;
    obj["str"] = "foo";
    obj["arr"] = arr;
    obj["void"] = QJsonValue();

    QJsonValue val = obj;
    std::string rep = R"({"arr":[42,"bar",1.23],"num":3,"str":"foo","void":null})";

    checkSerializationDeserealization(val, rep);
}

} // namespace test
