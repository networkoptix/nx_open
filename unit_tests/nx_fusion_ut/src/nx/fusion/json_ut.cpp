#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

QByteArray str(const QByteArray& source)
{
    return "\"" + source + "\"";
}

static const QByteArray kHelloWorld(str("hello world"));
}

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
