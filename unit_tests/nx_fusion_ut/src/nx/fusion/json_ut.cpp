#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

QByteArray str(const QByteArray& source)
{
    return "\"" + source + "\"";
}

static const QByteArray kHelloWorld(str("hello world"));

} // namespace

TEST_F(QnFusionTestFixture, integralTypes)
{
    ASSERT_EQ("5", QJson::serialized(5));
    ASSERT_EQ(5, QJson::deserialized<int>("5"));
    ASSERT_EQ("-12", QJson::serialized(-12));
    ASSERT_EQ(-12, QJson::deserialized<int>("-12"));
}

TEST_F(QnFusionTestFixture, QtStringTypes)
{
    //ASSERT_EQ(kHelloWorld, QJson::serialized(kHelloWorld)); -- not supported, returns base64
    ASSERT_EQ(kHelloWorld, QJson::serialized(QString("hello world")));
}

TEST_F(QnFusionTestFixture, stdStringTypes)
{
    // ASSERT_EQ(kHelloWorld, QJson::serialized("hello world")); -- not supported, return "true"
    ASSERT_EQ(kHelloWorld, QJson::serialized(std::string("hello world")));
}

TEST_F(QnFusionTestFixture, enumValue)
{
    const auto value = str("Flag0");
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(value, QJson::serialized(flag));
    ASSERT_EQ(flag, QJson::deserialized<nx::TestFlag>(value));
}

TEST_F(QnFusionTestFixture, flagsValue)
{
    const auto value = str("Flag1|Flag2");
    nx::TestFlags flags = nx::Flag1 | nx::Flag2;
    ASSERT_EQ(value, QJson::serialized(flags));
    ASSERT_EQ(flags, QJson::deserialized<nx::TestFlags>(value));
}

TEST_F(QnFusionTestFixture, invalidFlagsLexical)
{
    const auto value = str("Flag7");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_FALSE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag0, flags);
}

TEST_F(QnFusionTestFixture, flagsNumeric)
{
    const auto value = str("3");
    nx::TestFlags flags = nx::Flag0;
    ASSERT_TRUE(QJson::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag1|nx::Flag2, flags);
}

TEST_F(QnFusionTestFixture, bitArray)
{
    QBitArray value(17);
    value.setBit(1);
    value.setBit(5);
    value.setBit(6);
    value.setBit(9);
    value.setBit(11);
    value.setBit(16);

    const auto data = QJson::serialized(value);

    QBitArray target;
    ASSERT_TRUE(QJson::deserialize(data, &target));
    ASSERT_EQ(value, target);
}

namespace {

struct MockData
{
    int a = 661;
    int b = 662;
    int c = 663;

    MockData() = default;

    MockData(int a, int b, int c): a(a), b(b), c(c) {}

    bool operator==(const MockData& other) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    bool operator!=(const MockData& other) const { return !(*this == other); }

    std::string toJsonString() const { return QJson::serialized(*this).toStdString(); }

    static DeprecatedFieldNames* getDeprecatedFieldNames()
    {
        static DeprecatedFieldNames kDeprecatedFieldNames{
            {lit("b"), lit("deprecatedB")},
            {lit("c"), lit("deprecatedC")},
        };
        return &kDeprecatedFieldNames;
    }
};
#define MockData_Fields (a)(b)(c)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((MockData), (ubjson)(json), _Fields)

} // namespace

TEST_F(QnFusionTestFixture, deserializeStruct)
{
    const MockData data(113, 115, 117);

    const QByteArray jsonStr = R"json(
        {
            "a": 113,
            "b": 115,
            "c": 117
        }
    )json";

    MockData deserializedData;
    ASSERT_TRUE(QJson::deserialize(jsonStr, &deserializedData));
    ASSERT_EQ(data, deserializedData);
}

TEST_F(QnFusionTestFixture, deserializeStructWithDeprecatedFields)
{
    const MockData data(113, 115, 117);

    const QByteArray jsonStr = R"json(
        {
            "a": 113,
            "deprecatedB": 115,
            "deprecatedC": 117
        }
    )json";

    MockData deserializedData;
    ASSERT_TRUE(QJson::deserialize(jsonStr, &deserializedData));
    ASSERT_EQ(data, deserializedData);
}
