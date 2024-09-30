// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/rest/exception.h>
#include <nx/network/rest/json.h>
#include <nx/utils/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/device_model.h>

namespace nx::network::rest::json::test {

struct MockDataVariantA
{
    std::string a;

    bool operator==(const MockDataVariantA& other) const = default;
};
#define MockDataVariantA_Fields (a)

struct MockDataVariantB
{
    std::string b;

    bool operator==(const MockDataVariantB& other) const = default;
};
#define MockDataVariantB_Fields (b)

struct MockDataWithVariant
{
    std::variant<MockDataVariantA, MockDataVariantB> variantField;

    bool operator==(const MockDataWithVariant& other) const = default;
};
#define MockDataWithVariant_Fields (variantField)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MockDataVariantA, (json), MockDataVariantA_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MockDataVariantB, (json), MockDataVariantB_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MockDataWithVariant, (json), MockDataWithVariant_Fields)

static inline void filter(QJsonValue* value, Params filters)
{
    auto with = details::extractWithParam(&filters);
    details::filter(
        value, nullptr, DefaultValueAction::removeEqual, std::move(filters), std::move(with));
}

static inline void filter(QJsonValue* value, const QJsonValue* default_)
{
    details::filter(value, default_, DefaultValueAction::removeEqual);
}

TEST(Json, NestedObjects)
{
    {
        const QByteArray json = R"json({"info": 1, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue copy = value;
        test::filter(&value, {{"_with", "info,general"}});
        ASSERT_EQ(copy, value);
    }
    {
        const QByteArray json = R"json({"info": 1, "general": 0, "recording": 2})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue copy = value;
        test::filter(&value, {{"_with", "info"}});
        QJsonObject o = copy.toObject();
        o.remove("general");
        o.remove("recording");
        ASSERT_EQ(QJsonValue(o), value);
    }
    {
        const QByteArray json = R"json(
            [
                {"info": {"nested1": 0, "nested2": 1}, "general": 0, "recording": 2},
                {"info": {"nested2": 3}}
            ])json";
        const QByteArray jsonExpected = R"json(
            [
                {"info": {"nested2": 1}},
                {"info": {"nested2": 3}}
            ])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"_with", "info.nested2"}});
        QJsonValue expected;
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json({"recording": 1, "general": 0})json";
        const QByteArray jsonExpected = R"json({"general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"_with", "general,info,some"}});
        QJsonValue expected;
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json({"recording": 1, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue copy = value;
        ASSERT_THROW(test::filter(&value, {{"_with", "general,,info"}}), Exception);
        try
        {
            test::filter(&copy, {{"_with", "info,,general"}});
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message(), "Empty item in _with parameter");
        }
    }
}

TEST(Json, FilterValues)
{
    {
        const QByteArray json = R"json(
            [
                {"info": {"nested1": 0, "nested2": 1}, "general": 0, "recording": 2},
                {"info": {"nested2": 3}}
            ])json";
        const QByteArray jsonExpected = R"json(
            [
                {"info": {"nested1": 0, "nested2": 1}, "general": 0, "recording": 2}
            ])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"info.nested2", "1"}});
        QJsonValue expected;
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json(
            [
                {"info": {"nested1": 0, "nested2": 1}, "general": 0, "recording": 2},
                {"info": {"nested2": 1}, "general": "g"}
            ])json";
        const QByteArray jsonExpected = R"json(
            [
                {"info": {"nested2": 1}, "general": "g"}
            ])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"info.nested2", "1"}, {"general", "g"}});
        QJsonValue expected;
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json(
            [
                {"info": {"nested1": 0, "nested2": 1}, "general": 0, "recording": 2},
                {"info": {"nested2": 3}}
            ])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"info.nested2", "2"}});
        QJsonValue expected;
        const QByteArray jsonExpected = "[]";
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json({"info": {"nested1": 0, "nested2": 1}, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"info", {}}});
        ASSERT_EQ(QJsonValue(), value);
    }
    {
        const QByteArray json = R"json({"recording": 1, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        test::filter(&value, {{"info", {}}});
        ASSERT_EQ(QJsonValue(), value);
    }
    {
        QJsonValue value =
            QJsonArray({QJsonObject({{"general", 1}}), QJsonObject({{"recording", 2}})});
        test::filter(&value, {{"general", "2"}});
        ASSERT_TRUE(value.isArray() && value.toArray().isEmpty());
    }
    {
        QJsonValue value = QJsonArray(
            {QJsonObject({{"general", 0}, {"recording", 2}}), QJsonObject({{"general", 1}})});
        test::filter(&value, {{"recording", "1"}});
        ASSERT_TRUE(value.isArray() && value.toArray().isEmpty());
    }
}

TEST(Json, RemoveDefaultValues)
{
    {
        const QByteArray json = R"json({"info": {"nested1": 0, "nested2": 1}, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue defaultValue;
        QJson::deserialize(QByteArray(R"json({"general": 0})json"), &defaultValue);
        test::filter(&value, &defaultValue);
        QJsonValue expected;
        const QByteArray jsonExpected = R"json({"info": {"nested1": 0, "nested2": 1}})json";
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json({"info": {"nested1": 0, "nested2": 1}, "general": 0})json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue defaultValue;
        QJson::deserialize(QByteArray(R"json({"info": {"nested1": 1, "nested2": 1}})json"), &defaultValue);
        test::filter(&value, &defaultValue);
        QJsonValue expected;
        const QByteArray jsonExpected = R"json({"info": {"nested1": 0}, "general": 0})json";
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value);
    }
    {
        const QByteArray json = R"json([{
            "numbers": [1, 0, null],
            "strings": ["a", "", null],
            "objects": [{"v": 1}, {}, null],
            "arrays": [[1], [], null]
        }])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue defaultValue;
        test::filter(&value, &defaultValue);
        QJsonValue expected;
        QJson::deserialize(json, &expected);
        ASSERT_EQ(expected, value)
            << QJson::serialized(expected).toStdString() << '\n'
            << QJson::serialized(value).toStdString();
    }
    {
        const QByteArray json = R"json([{"emptyArray": []}])json";
        QJsonValue value;
        QJson::deserialize(json, &value);
        QJsonValue defaultValue;
        test::filter(&value, &defaultValue);
        QJsonValue expected;
        const QByteArray jsonExpected = R"json([{}])json";
        QJson::deserialize(jsonExpected, &expected);
        ASSERT_EQ(expected, value)
            << QJson::serialized(expected).toStdString() << '\n'
            << QJson::serialized(value).toStdString();
    }
}

TEST(Json, KeepDefault)
{
    using namespace nx::utils::json;
    const auto defaultValue = nx::vms::api::DeviceModel();
    const QJsonValue valueWithDefault =
        filter(defaultValue, {}, DefaultValueAction::appendMissing);
    const QJsonValue valueWoDefault =
        filter(defaultValue, {}, DefaultValueAction::removeEqual);
    ASSERT_EQ(getString(getObject(valueWithDefault, "options"), "failoverPriority"), "Medium");
    ASSERT_TRUE(optObject(valueWoDefault, "options").isEmpty());

    const auto defaultArray =
        std::vector<nx::vms::api::DeviceModel>({nx::vms::api::DeviceModel()});
    const QJsonArray arrayWithDefault =
        asArray(filter(defaultArray, {}, DefaultValueAction::appendMissing));
    const QJsonArray arrayWoDefault =
        asArray(filter(defaultArray, {}, DefaultValueAction::removeEqual));
    ASSERT_EQ(arrayWithDefault.size(), 1U);
    ASSERT_EQ(
        getString(getObject(arrayWithDefault[0], "options"), "failoverPriority"), "Medium");
    ASSERT_EQ(arrayWoDefault.size(), 1U);
    ASSERT_TRUE(asObject(arrayWoDefault.at(0)).isEmpty());

    const std::map<QString, QJsonValue> mapWithDefaultValues{
        {"b", false}, {"s", ""}, {"d", 0.}, {"o", QJsonObject()}, {"a", QJsonArray()}};
    const QJsonValue mapWithKeepDefault = json::filter(mapWithDefaultValues);
    ASSERT_FALSE(getBool(mapWithKeepDefault, "b"));
    ASSERT_TRUE(getString(mapWithKeepDefault, "s").isEmpty());
    ASSERT_EQ(getDouble(mapWithKeepDefault, "d"), 0.);
    ASSERT_TRUE(getObject(mapWithKeepDefault, "o").isEmpty());
    ASSERT_TRUE(getArray(mapWithKeepDefault, "a").isEmpty());
}

TEST(Json, KeepDefaultWithVariant)
{
    using namespace nx::utils::json;

    const auto defaultValueWithVariant = MockDataWithVariant();

    const QJsonValue variantWithDefault =
        filter(defaultValueWithVariant, {}, DefaultValueAction::appendMissing);
    EXPECT_EQ(getString(getObject(variantWithDefault, "variantField"), "a"), "");

    const QJsonValue variantWoDefault =
        filter(defaultValueWithVariant, {}, DefaultValueAction::removeEqual);
    // All fields with default values for for the field type are stripped.
    EXPECT_TRUE(asObject(variantWoDefault).empty());

    const auto lastVariant = MockDataWithVariant{.variantField = MockDataVariantB{.b = "1"}};

    const QJsonValue lastVariantWithDefault =
        filter(lastVariant, {}, DefaultValueAction::appendMissing);
    EXPECT_EQ(getString(getObject(lastVariantWithDefault, "variantField"), "b"), "1");
    EXPECT_FALSE(getObject(lastVariantWithDefault, "variantField").contains("a"));
    const QJsonValue lastVariantWoDefault =
        filter(lastVariant, {}, DefaultValueAction::removeEqual);
    // Not default value is not stripped.
    EXPECT_EQ(getString(getObject(lastVariantWoDefault, "variantField"), "b"), "1");
    EXPECT_FALSE(getObject(lastVariantWoDefault, "variantField").contains("a"));
}

TEST(Json, WithAndKeepDefault)
{
    using namespace nx::utils::json;

    const auto defaultValue = nx::vms::api::DeviceModel();
    const QJsonValue valueWithIdAndName =
        json::filter(defaultValue, {{"_with", "options"}});
    EXPECT_EQ(getString(getObject(valueWithIdAndName, "options"), "failoverPriority"), "Medium");
    EXPECT_FALSE(asObject(valueWithIdAndName).contains("id"));
}

TEST(Json, With)
{
    using namespace nx::utils::json;

    #define EXPECT_WITH(JSON, EXPECTED, WITH) \
    { \
        const QByteArray json = JSON; \
        const QByteArray jsonExpected = EXPECTED; \
        QJsonValue value; \
        QJson::deserialize(json, &value); \
        test::filter(&value, {{"_with", WITH}}); \
        QJsonValue expected; \
        QJson::deserialize(jsonExpected, &expected); \
        ASSERT_EQ(expected, value); \
    }

    EXPECT_WITH(
        R"json({"b": {"nested2": 0}})json",
        R"json({"b": {"nested2": 0}})json",
        "a.nothing,b.nested2");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0}})json",
        R"json({"a": {"nested1": 0}})json",
        "a.nested1,b.nested2");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        "*");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0}})json",
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0}})json",
        "*.nested1");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        R"json({"a": {"nested1": 0}})json",
        "*.nested1");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        R"json({"b": {"nested2": 0}})json",
        "*.nothing,b.nested2");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        "a.*,b.*");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0}})json",
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0}})json",
        "*.nested1,b.nested2");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested2": 0}})json",
        R"json({})json",
        "a.nested2,b.nested1");
    EXPECT_WITH(
        R"json({"a": {"nested1": 0}, "b": {"nested1": 0, "nested2": 0}})json",
        R"json({"b": {"nested1": 0, "nested2": 0}})json",
        "*.nested2,b");
    EXPECT_WITH(
        R"json({"a": 0, "b": {"nested2": 0}})json",
        R"json({"a": 0, "b": {"nested2": 0}})json",
        "a,*.nested2");
    EXPECT_WITH(
        R"json(
            [
                {"a": {"nested1": 0, "nested2": 1}, "b": {"nested2": 0}},
                {"a": {"nested1": 0}, "b": {"nested2": 0}}
            ])json",
        R"json(
            [
                {"a": {"nested2": 1}, "b": {"nested2": 0}},
                {"b": {"nested2": 0}}
            ])json",
        "*.nested2");

    #undef EXPECT_WITH
}

QJsonValue rawJsonToJsonValue(QByteArray json)
{
    QJsonValue value;
    NX_ASSERT(QJson::deserialize(json, &value));
    return value;
}

TEST(Json, replaceObjectWithNotObject)
{
    auto existingValue = rawJsonToJsonValue(R"json(
        {
            "params": {"settings": {"innerInt": 1}},
            "id": "id1",
            "params2": {}
        })json");
    QString error;

    {
        auto incompleteValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": "string_value"}
            })json");
        auto expectedValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": "string_value"},
                "id": "id1",
                "params2": {}
            })json");
        QJsonValue mergedValue;
        ASSERT_TRUE(merge(&mergedValue, existingValue, incompleteValue, &error, /*chronoSerializedAsDouble*/ true));
        ASSERT_EQ(mergedValue, expectedValue)
            << QJson::serialized(mergedValue).toStdString() << "!=" << QJson::serialized(expectedValue).toStdString();
    }

    {
        auto incompleteValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": null}
            })json");
        auto expectedValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": null},
                "id": "id1",
                "params2": {}
            })json");
        QJsonValue mergedValue;
        ASSERT_TRUE(merge(&mergedValue, existingValue, incompleteValue, &error, /*chronoSerializedAsDouble*/ true));
        ASSERT_EQ(mergedValue, expectedValue)
            << QJson::serialized(mergedValue).toStdString() << "!=" << QJson::serialized(expectedValue).toStdString();
    }

    {
        auto incompleteValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": {"innerString": "innner_string"} }
            })json");
        auto expectedValue = rawJsonToJsonValue(R"json(
            {
                "params": {"settings": {"innerInt": 1, "innerString": "innner_string"} },
                "id": "id1",
                "params2": {}
            })json");
        QJsonValue mergedValue;
        ASSERT_TRUE(merge(&mergedValue, existingValue, incompleteValue, &error, /*chronoSerializedAsDouble*/ true));
        ASSERT_EQ(mergedValue, expectedValue)
            << QJson::serialized(mergedValue).toStdString() << "!=" << QJson::serialized(expectedValue).toStdString();
    }
}

struct TestModel
{
    QString id;
    std::map<QString, QString> params;
    bool operator==(const TestModel& rhs) const = default;
};
QN_FUSION_ADAPT_STRUCT(TestModel, (id)(params))
QN_FUSION_DEFINE_FUNCTIONS(TestModel, (json))

TEST(Json, replacedObjectFailsDeserialization)
{
    const TestModel existingValue{"id1", {{"inner", "innerValue"}}};
    QString error;
    {
        auto incompleteValue = rawJsonToJsonValue(R"json(
            {
                "params": "string"
            })json");
        TestModel mergedValue;
        ASSERT_FALSE(merge(&mergedValue, existingValue, incompleteValue, &error, /*chronoSerializedAsDouble*/ true));
    }
    {
        auto incompleteValue = rawJsonToJsonValue(R"json(
            {
                "params": {"inner2": "innerValue2"}
            })json");
        TestModel mergedValue;
        ASSERT_TRUE(merge(&mergedValue, existingValue, incompleteValue, &error, /*chronoSerializedAsDouble*/ true));
        auto expectedValue = existingValue;
        expectedValue.params["inner2"] = "innerValue2";
        ASSERT_EQ(mergedValue, expectedValue);
    }
}

} // namespace nx::network::rest::json::test
