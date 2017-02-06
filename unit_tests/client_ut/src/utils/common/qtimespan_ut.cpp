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

}

TEST(QTimeSpanTest, approximateString)
{
    QTimeSpan s(kExampleTime);
    ASSERT_EQ("1 minutes, 15 seconds", s.toApproximateString().toStdString());
}

TEST(QTimeSpanTest, approximateStringNegative)
{
    qint64 negativeMs = -1 * kExampleTime.count();
    QTimeSpan s(negativeMs);
    ASSERT_EQ("-1 minutes, 15 seconds", s.toApproximateString().toStdString());
}

TEST(QTimeSpanTest, shortApproximateString)
{
    QTimeSpan s(kExampleTime);
    ASSERT_EQ("1m 15s", s.toApproximateString(kDoNotSuppress,
        kFormat,
        QTimeSpan::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}


TEST(QTimeSpanTest, longApproximateString)
{
    QTimeSpan s(kExampleTime);
    ASSERT_EQ("1 min 15 sec", s.toApproximateString(kDoNotSuppress,
        kFormat,
        QTimeSpan::SuffixFormat::Long,
        kSeparator)
        .toStdString());
}

TEST(QTimeSpanTest, shortApproximateStringNegative)
{
    qint64 negativeMs = -1 * kExampleTime.count();
    QTimeSpan s(negativeMs);
    ASSERT_EQ("-1m 15s", s.toApproximateString(kDoNotSuppress,
        kFormat,
        QTimeSpan::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST(QTimeSpanTest, smallApproximateStringNegative)
{
    /* Check if small negative value correctly rounded to non-signed zero */
    qint64 negativeMs = -1;
    QTimeSpan s(negativeMs);
    ASSERT_EQ("0s", s.toApproximateString(kDoNotSuppress,
        kFormat,
        QTimeSpan::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}
