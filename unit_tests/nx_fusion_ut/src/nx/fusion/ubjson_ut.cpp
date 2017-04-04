#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

TEST(UbJsonRectTest, float)
{
    float value = 17.2f;
    QByteArray data = QnUbjson::serialized(value);
    float result = QnUbjson::deserialized<float>(data);
    ASSERT_TRUE(qFuzzyEquals(value, result));
}

TEST(UbJsonRectTest, double)
{
    const double value = 17.2;
    QByteArray data = QnUbjson::serialized(value);
    const double result = QnUbjson::deserialized<double>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonRectTest, QRect)
{
    QRect value(1, 2, 5, 17);
    QByteArray data = QnUbjson::serialized(value);
    QRect result = QnUbjson::deserialized<QRect>(data);
    ASSERT_EQ(value, result);
}

TEST(UbJsonRectTest, QRectF)
{
    QRectF value(1, 2, 5.5, 17.2);
    QByteArray data = QnUbjson::serialized(value);
    QRectF result = QnUbjson::deserialized<QRectF>(data);
    ASSERT_EQ(value, result);
}
