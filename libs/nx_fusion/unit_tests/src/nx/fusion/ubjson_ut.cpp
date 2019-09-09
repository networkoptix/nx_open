#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

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

    bool operator==(const BriefMockDataWithQJsonValue& other) const
    {
        return pre == other.pre
            && nullVal == other.nullVal
            && t == other.t
            && tVal == other.tVal
            && f == other.f
            && fVal == other.fVal
            && d == other.d
            && dVal == other.dVal
            && s == other.s
            && sVal == other.sVal
            && post == other.post;
    }
};
#define BriefMockDataWithQJsonValue_Fields (pre)(nullVal)(t)(tVal)(f)(fVal)(d)(dVal)(s)(sVal)(post)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((BriefMockDataWithQJsonValue), (json), _Fields)

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

    BriefMockDataWithQJsonValue result = QJson::deserialized<BriefMockDataWithQJsonValue>(
        QJson::serialized(value));

    ASSERT_EQ(value, result);
}