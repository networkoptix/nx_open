#include <gtest/gtest.h>

#include <chrono>

#include <QtCore/QDebug>

#include <utils/common/qtimespan.h>

using namespace std::chrono;

namespace {

static const milliseconds kExampleTime = minutes(1) + seconds(15);
static const int kDoNotSuppress = -1;
static const QString kSeparator(L' ');
static const Qt::TimeSpanFormat kFormat = Qt::Seconds | Qt::Minutes;

auto kConverter = [](Qt::TimeSpanUnit unit, int num)
    {
        QString suffix;
        switch (unit)
        {
            case::Qt::Seconds:
                suffix = L's';
                break;
            case::Qt::Minutes:
                suffix = L'm';
                break;
            default:
                break;
        }

        return QString::number(num) + suffix;
    };

}

TEST(QTimeSpanTest, approximateString)
{
    QTimeSpan s(kExampleTime);
    ASSERT_EQ(s.toApproximateString().toStdString(), "1 minute(s), 15 second(s)");
}

TEST(QTimeSpanTest, approximateStringNegative)
{
    qint64 negativeMs = -1 * kExampleTime.count();
    QTimeSpan s(negativeMs);
    ASSERT_EQ(s.toApproximateString().toStdString(), "-1 minute(s), 15 second(s)");
}

TEST(QTimeSpanTest, customApproximateString)
{
    QTimeSpan s(kExampleTime);
    ASSERT_EQ(s.toApproximateString(kDoNotSuppress, kFormat, kConverter, kSeparator)
        .toStdString(), "1m 15s");
}

TEST(QTimeSpanTest, customApproximateStringNegative)
{
    qint64 negativeMs = -1 * kExampleTime.count();
    QTimeSpan s(negativeMs);
    ASSERT_EQ(s.toApproximateString(kDoNotSuppress, kFormat, kConverter, kSeparator)
        .toStdString(), "-1m 15s");
}

TEST(QTimeSpanTest, smallApproximateStringNegative)
{
    /* Check if small negative value correctly rounded to non-signed zero */
    qint64 negativeMs = -1;
    QTimeSpan s(negativeMs);
    ASSERT_EQ(s.toApproximateString(kDoNotSuppress, kFormat, kConverter, kSeparator)
        .toStdString(), "0s");
}
