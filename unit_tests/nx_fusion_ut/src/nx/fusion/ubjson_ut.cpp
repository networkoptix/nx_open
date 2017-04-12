#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

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
