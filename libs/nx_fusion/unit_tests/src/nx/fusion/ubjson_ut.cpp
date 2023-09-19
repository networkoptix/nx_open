// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

struct TestStruct
{
    int intData = 0;
    QString stringData;
    double doubleData = 0.0;
};

#define TestStruct_Fields (intData)(stringData)(doubleData)

struct TestStructEx: public TestStruct
{
    QnUuid additionalField;
};

#define TestStructEx_Fields TestStruct_Fields (additionalField)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestStruct, (ubjson), TestStruct_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestStructEx, (ubjson), TestStructEx_Fields)

struct BriefMockDataWithQJsonValue
{
    int pre = 666;
    QJsonValue nullVal;
    bool t = true;
    QJsonValue tVal;
    bool f = false;
    QJsonValue fVal;
    double d = 123.45;
    QJsonValue dVal;
    QString s = "Hello world";
    QJsonValue sVal;
    int post = 999;
    QJsonValue arrVal;
    QJsonValue objVal;

    bool operator==(const BriefMockDataWithQJsonValue& other) const = default;
};
#define BriefMockDataWithQJsonValue_Fields (pre)(nullVal)(t)(tVal)(f)(fVal)(d)(dVal)(s)(sVal) \
    (post)(arrVal)(objVal)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BriefMockDataWithQJsonValue, (ubjson), BriefMockDataWithQJsonValue_Fields)

TEST(UbJsonTest, qint8)
{
    const qint8 value = -128;
    const QByteArray data = QnUbjson::serialized(value);
    const qint8 result = QnUbjson::deserialized<qint8>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, float)
{
    const float value = 17.2f;
    const QByteArray data = QnUbjson::serialized(value);
    const float result = QnUbjson::deserialized<float>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, double)
{
    const double value = 17.2;
    const QByteArray data = QnUbjson::serialized(value);
    const double result = QnUbjson::deserialized<double>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, QRect)
{
    const QRect value(1, 2, 5, 17);
    const QByteArray data = QnUbjson::serialized(value);
    const QRect result = QnUbjson::deserialized<QRect>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, QRectF)
{
    const QRectF value(1, 2, 5.5, 17.2);
    const QByteArray data = QnUbjson::serialized(value);
    const QRectF result = QnUbjson::deserialized<QRectF>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, chronoTypes)
{
    const std::chrono::seconds s(3);
    const QByteArray dataS = QnUbjson::serialized(s);
    const std::chrono::seconds resultS = QnUbjson::deserialized<std::chrono::seconds>(dataS);
    ASSERT_EQ(s, resultS);

    const std::chrono::milliseconds ms(5);
    const QByteArray dataMs = QnUbjson::serialized(ms);
    const std::chrono::milliseconds resultMs = QnUbjson::deserialized<std::chrono::milliseconds>(dataMs);
    ASSERT_EQ(ms, resultMs);

    const std::chrono::microseconds us(7);
    const QByteArray dataUs = QnUbjson::serialized(us);
    const std::chrono::microseconds resultUs = QnUbjson::deserialized<std::chrono::microseconds>(dataUs);
    ASSERT_EQ(us, resultUs);
}

TEST(UbJsonTest, qJsonValue)
{
    BriefMockDataWithQJsonValue value;
    value.tVal = value.t;
    value.fVal = value.f;
    value.dVal = value.d;
    value.sVal = value.s;
    value.arrVal = QJsonArray({ 1, 2 });
    value.objVal = QJsonObject({ {"key", "val"} });

    BriefMockDataWithQJsonValue result = QnUbjson::deserialized<BriefMockDataWithQJsonValue>(
        QnUbjson::serialized(value));

    ASSERT_EQ(value, result);
}

TEST(UbJsonTest, qJsonValueComplex)
{
    const QJsonArray jsonArray = {
        QJsonValue(), QJsonValue(true), 3, "hi", 1.01
    };

    const QJsonObject jsonObject = {
        {"nul", QJsonValue()},
        {"bool", true},
        {"int", 3},
        {"double", 1.01},
        {"str", "hi"}
    };

    auto jsonArrayComplex = jsonArray;
    jsonArrayComplex.push_back(jsonArray);
    jsonArrayComplex.push_back(jsonObject);

    auto jsonObjectComplex = jsonObject;
    jsonObjectComplex["arr"] = jsonArrayComplex;
    jsonObjectComplex["obj"] = jsonObject;

    const auto expected = QJsonValue(jsonObjectComplex);
    SCOPED_TRACE(QJson::serialized(expected).toStdString());

    const auto ubody = QnUbjson::serialized(expected);
    ASSERT_GT(ubody.size(), 0);

    bool ok = false;
    const auto actual = QnUbjson::deserialized(ubody, QJsonValue(), &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(expected, actual);
}

TEST(UbJsonTest, customStructDescendantFromAncestor)
{
    TestStruct testStruct{
        2,
        "some text data",
        3.1
    };

    const QByteArray data = QnUbjson::serialized(testStruct);
    const TestStructEx result = QnUbjson::deserialized<TestStructEx>(data);

    ASSERT_EQ(result.intData, testStruct.intData);
    ASSERT_EQ(result.stringData, testStruct.stringData);
    ASSERT_DOUBLE_EQ(result.doubleData, testStruct.doubleData);
}

TEST(UbJsonTest, customStructVectorDescendantFromAncestor)
{
    std::vector<TestStruct> testStructures = {
        {2, "some text data_1", 3.1},
        {1, "some text data_2", 4.1},
        {5, "some text data_3", 5.1}
    };

    const QByteArray data = QnUbjson::serialized(testStructures);
    const std::vector<TestStructEx> result =
        QnUbjson::deserialized<std::vector<TestStructEx>>(data);

    ASSERT_EQ(result.size(), testStructures.size());

    for (int i = 0; i < testStructures.size(); ++i)
    {
        ASSERT_EQ(result[i].intData, testStructures[i].intData);
        ASSERT_EQ(result[i].stringData, testStructures[i].stringData);
        ASSERT_DOUBLE_EQ(result[i].doubleData, testStructures[i].doubleData);
    }
}

TEST(UbJsonTest, customStructAncestorFromDescendant)
{
    TestStructEx testStruct{
        2,
        "some text data",
        3.1,
        QnUuid::createUuid()
    };

    const QByteArray data = QnUbjson::serialized(testStruct);
    const TestStruct result = QnUbjson::deserialized<TestStruct>(data);

    ASSERT_EQ(result.intData, testStruct.intData);
    ASSERT_EQ(result.stringData, testStruct.stringData);
    ASSERT_DOUBLE_EQ(result.doubleData, testStruct.doubleData);
}

TEST(UbJsonTest, customStructVectorAncestorFromDescendant)
{
    std::vector<TestStructEx> testStructures = {
        {2, "some text data_1", 3.1, QnUuid::createUuid()},
        {1, "some text data_2", 4.1, QnUuid::createUuid()},
        {5, "some text data_3", 5.1, QnUuid::createUuid()}
    };

    const QByteArray data = QnUbjson::serialized(testStructures);
    const std::vector<TestStruct> result = QnUbjson::deserialized<std::vector<TestStruct>>(data);

    ASSERT_EQ(result.size(), testStructures.size());

    for (int i = 0; i < testStructures.size(); ++i)
    {
        ASSERT_EQ(result[i].intData, testStructures[i].intData);
        ASSERT_EQ(result[i].stringData, testStructures[i].stringData);
        ASSERT_DOUBLE_EQ(result[i].doubleData, testStructures[i].doubleData);
    }
}
