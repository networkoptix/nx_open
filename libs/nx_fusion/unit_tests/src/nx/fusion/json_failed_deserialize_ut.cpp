// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

namespace {

NX_REFLECTION_ENUM_CLASS(Flag, SomeFlag, AnotherFlag)

struct GeneralData
{
    bool booleanValue = false;
    Flag flag = Flag::SomeFlag;
    nx::Uuid id;
    QString someString;
};

#define GeneralData_Fields (booleanValue)(flag)(id)(someString)

struct ListData
{
    QByteArray value;
};

#define ListData_Fields (value)

struct AttributesData
{
    double doubleNumber = 123.456789;
    Flag flag = Flag::AnotherFlag;
    int intNumber = 123;
    std::vector<ListData> list;
};

#define AttributesData_Fields (doubleNumber)(flag)(intNumber)(list)

struct Model
{
    std::optional<AttributesData> attributes;
    GeneralData general;
    std::map<QString, QJsonValue> parameters;
};

#define Model_Fields (attributes)(general)(parameters)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(GeneralData, (json)(xml), GeneralData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ListData, (json)(xml), ListData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AttributesData, (json)(xml), AttributesData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Model, (json)(xml), Model_Fields)

} // anonymous namespace

namespace nx::vms::api::test {

TEST(NxFusion, JsonFailedDeserializeStringAsDouble)
{
    static const QByteArray kJson = R"json({
        "general": {
            "booleanValue": false,
            "flag": "SomeFlag",
            "id": "{00000000-0000-0000-0000-000000000000}",
            "someString": ""
        },
        "parameters": {
            "array": [
                "item2",
                "item1"
            ],
            "object": {
                "object_key": "object_value"
            }
        },
        "attributes": {
            "flag": "AnotherFlag",
            "intNumber": 123,
            "list": [
                {
                    "_comment": "QByteArray value is serialized as Base64",
                    "value": "dmFsdWUy"
                },
                {
                    "_comment": "QByteArray value is serialized as Base64",
                    "value": "dmFsdWUx"
                }
            ],
            "doubleNumber": "123.456789"
        }
    })json";
    Model model;
    QnJsonContext ctx;
    ctx.setStrictMode(true);
    ASSERT_FALSE(QJson::deserialize(&ctx, kJson, &model));
    const auto failed = ctx.getFailedKeyValue();
    ASSERT_EQ(failed.first, "attributes.doubleNumber");
    ASSERT_EQ(failed.second, "\"123.456789\"");
}

#define EXPECT_JSON_EXCEPTION(TYPE, VALUE, EXCEPTION, MESSAGE) \
    try \
    { \
        QJson::deserializeOrThrow<TYPE>(QByteArray(VALUE)); \
        FAIL() << "Exception was not thrown"; \
    } \
    catch (const nx::json::EXCEPTION& e) \
    { \
        EXPECT_EQ(e.message(), MESSAGE); \
    } \
    catch (const std::exception& e) \
    { \
        FAIL() << NX_FMT("Unexpected exception %1: %2", &e, e.what()).toStdString(); \
    }

#define EXPECT_INVALID_JSON(T, V, M) EXPECT_JSON_EXCEPTION(T, V, InvalidJsonException, M)
#define EXPECT_INVALID_PARAMETER(T, V, M) EXPECT_JSON_EXCEPTION(T, V, InvalidParameterException, M)

TEST(NxFusion, JsonFailedDeserializeIntAsByteArray)
{
    static const QByteArray kJson = R"json({
        "general": {
            "booleanValue": false,
            "flag": "SomeFlag",
            "id": "{00000000-0000-0000-0000-000000000000}",
            "someString": ""
        },
        "parameters": {
            "array": [
                "item2",
                "item1"
            ],
            "object": {
                "object_key": "object_value"
            }
        },
        "attributes": {
            "doubleNumber": 123.456789,
            "flag": "AnotherFlag",
            "intNumber": 123,
            "list": [
                {
                    "_comment": "QByteArray value is serialized as Base64",
                    "value": "dmFsdWUy"
                },
                {
                    "value": 1
                }
            ]
        }
    })json";
    EXPECT_INVALID_PARAMETER(Model, kJson, "Invalid parameter 'attributes.list.value': 1");
}

TEST(NxFusion, JsonFailedDeserializeInvalidJson)
{
    EXPECT_INVALID_JSON(QJsonValue, "tru", R"(Unable to parse a boolean from JSON: "tru")");
    EXPECT_INVALID_JSON(QJsonValue, "nu", R"(Unable to parse a null value from JSON: "nu")");
    EXPECT_INVALID_JSON(QJsonValue, "{1", "Unable to parse a JSON object: unterminated object at 2");
    EXPECT_INVALID_JSON(QJsonValue, "[123", "Unable to parse a JSON array: invalid termination by number at 4");

    // Backslash in R-strings causes an error in MSVC.
    EXPECT_INVALID_JSON(QJsonValue, "\"not string", "Unable to parse a string from JSON: \"\\\"not string\"");
}

TEST(NxFusion, JsonFailedDeserializeWrongJson)
{
    EXPECT_INVALID_PARAMETER(int, "true", "Invalid value type");
    EXPECT_INVALID_PARAMETER(QString, "1", "Invalid value type");

    using IntDict = std::map<QString, int>;
    EXPECT_INVALID_PARAMETER(IntDict, "[1, 2, 3]", "Invalid value type");

    using IntArray = std::vector<int>;
    EXPECT_INVALID_PARAMETER(IntArray, R"({"a": 1, "b": 2})", "Invalid value type");

    EXPECT_INVALID_PARAMETER(GeneralData, "777", "Invalid value type");
}

} // namespace nx::vms::api::test
