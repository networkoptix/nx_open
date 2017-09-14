#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

static const QnUuid kTestId("b4a5d7ec-1952-4225-96ed-a08eaf34d97a");
static const QString kHelloWorld("hello world");

} // namespace

class QnLexicalTextFixture: public QnFusionTestFixture
{};

TEST_F(QnLexicalTextFixture, integralTypes)
{
    ASSERT_EQ("5", QnLexical::serialized(5));
    ASSERT_EQ(5, QnLexical::deserialized<int>("5"));
    ASSERT_EQ("-12", QnLexical::serialized(-12));
    ASSERT_EQ(-12, QnLexical::deserialized<int>("-12"));
}

TEST_F(QnLexicalTextFixture, QtStringTypes)
{
    //ASSERT_EQ(kHelloWorld, QnLexical::serialized(kHelloWorld.toUtf8())); -- not supported, returns base64
    ASSERT_EQ(kHelloWorld, QnLexical::serialized(kHelloWorld));
}

TEST_F(QnLexicalTextFixture, stdStringTypes)
{
    // ASSERT_EQ(kHelloWorld, QnLexical::serialized("hello world")); -- not supported, return "true"
    ASSERT_EQ(kHelloWorld, QnLexical::serialized(kHelloWorld.toStdString()));
}

TEST_F(QnLexicalTextFixture, enumValue)
{
    const QString value = "Flag0";
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(value, QnLexical::serialized(flag));
    ASSERT_EQ(flag, QnLexical::deserialized<nx::TestFlag>(value));
}

TEST_F(QnLexicalTextFixture, enumValueCaseSensitive)
{
    const QString value = "Flag0";
    nx::TestFlag flag = nx::Flag0;
    ASSERT_EQ(flag, QnLexical::deserialized<nx::TestFlag>(value.toLower()));
    ASSERT_EQ(flag, QnLexical::deserialized<nx::TestFlag>(value.toUpper()));
}

TEST_F(QnLexicalTextFixture, flagsValue)
{
    const QString value = "Flag1|Flag2";
    nx::TestFlags flags = nx::Flag1 | nx::Flag2;
    ASSERT_EQ(value, QnLexical::serialized(flags));
    ASSERT_EQ(flags, QnLexical::deserialized<nx::TestFlags>(value));
}

TEST_F(QnLexicalTextFixture, invalidFlagsLexical)
{
    const QString value = "Flag7";
    nx::TestFlags flags = nx::Flag0;
    ASSERT_FALSE(QnLexical::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag0, flags);
}

TEST_F(QnLexicalTextFixture, flagsNumeric)
{
    const QString value = "3";
    nx::TestFlags flags = nx::Flag0;
    ASSERT_TRUE(QnLexical::deserialize(value, &flags));
    ASSERT_EQ(nx::Flag1|nx::Flag2, flags);
}

TEST_F(QnLexicalTextFixture, bitArray)
{
    QBitArray value(17);
    value.setBit(1);
    value.setBit(5);
    value.setBit(6);
    value.setBit(9);
    value.setBit(11);
    value.setBit(16);

    const auto data = QnLexical::serialized(value);
    ASSERT_EQ(data, "YgoBAQ=="); //< Base64 is expected.

    QBitArray target;
    ASSERT_TRUE(QnLexical::deserialize(data, &target));
    ASSERT_EQ(value, target);
}

TEST_F(QnLexicalTextFixture, QRect)
{
    const QRect value(1, 2, 5, 17);
    const auto data = QnLexical::serialized(value);
    const QRect result = QnLexical::deserialized<QRect>(data);
    ASSERT_EQ(value, result);
}

TEST_F(QnLexicalTextFixture, QRectF)
{
    const QRectF value(1, 2, 5.5, 17.2);
    const auto data = QnLexical::serialized(value);
    const QRectF result = QnLexical::deserialized<QRectF>(data);
    ASSERT_EQ(value, result);
}
