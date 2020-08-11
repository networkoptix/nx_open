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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((TestStruct)(TestStructEx), (ubjson), _Fields)


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
    const std::chrono::milliseconds value(1000);
    const QByteArray data = QnUbjson::serialized(value);
    const std::chrono::milliseconds result = QnUbjson::deserialized<std::chrono::milliseconds>(data);
    ASSERT_EQ(value, result);
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

