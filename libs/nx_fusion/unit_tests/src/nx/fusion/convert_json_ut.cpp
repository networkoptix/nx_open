// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

namespace {

NX_REFLECTION_ENUM_CLASS(Flag, SomeFlag, AnotherFlag)

struct GeneralOrdered
{
    bool booleanValue = false;
    Flag flag = Flag::SomeFlag;
    nx::Uuid id;
    QString someString;
};

#define GeneralOrdered_Fields (booleanValue)(flag)(id)(someString)

struct ListData
{
    QByteArray value;
};

#define ListData_Fields (value)

struct AttributesOrdered
{
    double doubleNumber = 123.456789;
    Flag flag = Flag::AnotherFlag;
    int intNumber = 123;
    std::vector<ListData> list;
};

#define AttributesOrdered_Fields (doubleNumber)(flag)(intNumber)(list)

struct ModelOrdered
{
    std::optional<AttributesOrdered> attributes;
    GeneralOrdered general;
    std::map<QString, QJsonValue> parameters;
};

#define ModelOrdered_Fields (attributes)(general)(parameters)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(GeneralOrdered, (csv_record)(json)(xml), GeneralOrdered_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ListData, (csv_record)(json)(xml), ListData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AttributesOrdered, (csv_record)(json)(xml), AttributesOrdered_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ModelOrdered, (csv_record)(json)(xml), ModelOrdered_Fields)

} // anonymous namespace

namespace nx::vms::api::test {

TEST(NxFusion, JsonToXml)
{
    ModelOrdered model;
    model.attributes = AttributesOrdered{};
    model.attributes->list = {{"value2"}, {"value1"}};
    model.parameters["object"] = QJsonObject{{"object_key", "object_value"}};
    model.parameters["array"] = QJsonArray{"item2", "item1"};
    QJsonValue json;
    QJson::serialize(model, &json);
    const auto expected = QnXml::serialized(model, QStringLiteral("root"));
    const auto converted = QnXml::serialized(json, QStringLiteral("root"));
    ASSERT_EQ(converted, expected);
}

TEST(NxFusion, JsonToCsv)
{
    ModelOrdered model;
    model.attributes = AttributesOrdered{};
    model.attributes->list = {{"value2"}, {"value1"}};
    model.parameters["object"] = QJsonObject{{"object_key", "object_value"}};
    model.parameters["array"] = QJsonArray{"item2", "item1"};
    QJsonValue json;
    QJson::serialize(std::vector<ModelOrdered>{model, ModelOrdered{}}, &json);
    const QByteArray expected =
        "attributes.doubleNumber,"
        "attributes.flag,"
        "attributes.intNumber,"
        "general.booleanValue,"
        "general.flag,"
        "general.id,"
        "general.someString,"
        "parameters.object.object_key"
        "\r\n"
        "123.456789,"
        "AnotherFlag,"
        "123,"
        "false,"
        "SomeFlag,"
        "{00000000-0000-0000-0000-000000000000},"
        ","
        "object_value"
        "\r\n"
        ","
        ","
        ","
        "false,"
        "SomeFlag,"
        "{00000000-0000-0000-0000-000000000000},"
        ","
        ""
        "\r\n";
    QByteArray converted;
    QnCsvStreamWriter<QByteArray> stream(&converted);
    serialize(json, &stream);
    ASSERT_EQ(converted, expected);
}

} // namespace nx::vms::api::test
