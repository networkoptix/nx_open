// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <nx/fusion/nx_fusion_test_fixture.h>

namespace {

static const nx::Uuid kTestId("b4a5d7ec-1952-4225-96ed-a08eaf34d97a");
static const QString kHelloWorld("hello world");

} // namespace

class QnLexicalTextFixture: public QnFusionTestFixture
{};

TEST_F(QnLexicalTextFixture, boolTypes)
{
    EXPECT_EQ("true", QnLexical::serialized(true).toStdString());
    EXPECT_EQ(true, QnLexical::deserialized<bool>("true"));
    EXPECT_EQ(true, QnLexical::deserialized<bool>("1"));

    EXPECT_EQ("false", QnLexical::serialized(false).toStdString());
    EXPECT_EQ(false, QnLexical::deserialized<bool>("false"));
    EXPECT_EQ(false, QnLexical::deserialized<bool>("0"));
}

TEST_F(QnLexicalTextFixture, integralTypes)
{
    ASSERT_EQ("5", QnLexical::serialized(5));
    ASSERT_EQ(5, QnLexical::deserialized<int>("5"));
    ASSERT_EQ("-12", QnLexical::serialized(-12));
    ASSERT_EQ(-12, QnLexical::deserialized<int>("-12"));
}

TEST_F(QnLexicalTextFixture, chronoTypes)
{
    ASSERT_EQ("7", QnLexical::serialized(std::chrono::milliseconds(7)));
    ASSERT_EQ(std::chrono::milliseconds(50), QnLexical::deserialized<std::chrono::milliseconds>("50"));
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

TEST_F(QnLexicalTextFixture, negativeQRect)
{
    const QRect value(-1, -2, 5, 17);
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

TEST_F(QnLexicalTextFixture, negativeQRectF)
{
    const QRectF value(-1, -2, 5.5, 17.2);
    const auto data = QnLexical::serialized(value);
    const QRectF result = QnLexical::deserialized<QRectF>(data);
    ASSERT_EQ(value, result);
}

TEST_F(QnLexicalTextFixture, notDefaultLocale)
{
    QRectF rect(0.2, 0.3, 0.4, 0.5);
    const auto serializedData = QJson::serialized(rect);
    QRectF newRect = QJson::deserialized<QRectF>(serializedData);
    ASSERT_EQ(rect, newRect);

    const auto previousLocale = setlocale(LC_ALL, NULL);
    nx::utils::ScopeGuard guard = [&]() { setlocale(LC_ALL, previousLocale); };

    setlocale(LC_ALL, "rus"); //< This locale has comma delimiter for float numbers.
    const auto serializedData2 = QJson::serialized(rect);
    ASSERT_EQ(serializedData, serializedData2);
    newRect = QJson::deserialized<QRectF>(serializedData2);
    ASSERT_EQ(rect, newRect);
}
