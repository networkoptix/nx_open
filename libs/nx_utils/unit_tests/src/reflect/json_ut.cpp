// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <nx/utils/serialization/qjson.h>

namespace test {

struct Foo
{
    int num = 0;
    QJsonValue qJson;

};

NX_REFLECTION_INSTRUMENT(Foo, (num)(qJson))

class Json: public ::testing::Test
{
public:
    void thenDeserializedCorrect(
        const nx::reflect::DeserializationResult& result, const Foo& expected, const Foo& obtained)
    {
        ASSERT_TRUE(result.success);
        ASSERT_EQ(expected.num, obtained.num);
        ASSERT_EQ(expected.qJson, obtained.qJson);
    }

    void checkSerializationDeserealization(const Foo& obj, const std::string& strRepresentation)
    {
        auto str = nx::reflect::json::serialize(obj);
        ASSERT_EQ(strRepresentation, str);
        Foo resultObj;
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
    checkSerializationDeserealization(f, R"({"num":10,"qJson":[1.0,2.0,3.0,4.0,[1.0,2.0]]})");
}

TEST_F(Json, qjson_int_support)
{
    Foo f{12, QJsonValue(12)};
    checkSerializationDeserealization(f, R"({"num":12,"qJson":12.0})");
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

} // namespace test
