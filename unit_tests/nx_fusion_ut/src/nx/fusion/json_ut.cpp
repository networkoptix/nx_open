#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {
static const QByteArray kHelloWorld("\"hello world\"");
}

TEST_F(QnFusionTestFixture, integralTypes)
{
    ASSERT_EQ("5", QJson::serialized(5));
    ASSERT_EQ("-12", QJson::serialized(-12));
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
