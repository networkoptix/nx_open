#include <gtest/gtest.h>

#include <nx/client/core/utils/human_readable.h>
#include <chrono>

namespace nx {
namespace client {
namespace core {

using namespace std::chrono;

namespace {

static const milliseconds kExampleTime = minutes(1) + seconds(15);
static const QString kSeparator(L' ');
static const auto kFormat = HumanReadable::Seconds | HumanReadable::Minutes;

} // namespace


TEST(HumanReadableTest, timeSpanNoUnit)
{
    ASSERT_TRUE(HumanReadable::timeSpan(kExampleTime, HumanReadable::NoUnit).isEmpty());
}

TEST(HumanReadableTest, timeSpan)
{
    ASSERT_EQ("1 minutes, 15 seconds", HumanReadable::timeSpan(kExampleTime).toStdString());
}

TEST(HumanReadableTest, timeSpanNegative)
{
    const auto negativeMs = milliseconds(-1 * kExampleTime.count());
    ASSERT_EQ("-1 minutes, 15 seconds", HumanReadable::timeSpan(negativeMs).toStdString());
}

TEST(HumanReadableTest, shortTimeSpan)
{
    ASSERT_EQ("1m 15s", HumanReadable::timeSpan(kExampleTime,
        kFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}


TEST(HumanReadableTest, longTimeSpan)
{
    ASSERT_EQ("1 min 15 sec", HumanReadable::timeSpan(kExampleTime,
        kFormat,
        HumanReadable::SuffixFormat::Long,
        kSeparator)
        .toStdString());
}

TEST(HumanReadableTest, shortTimeSpanNegative)
{
    const auto negativeMs = milliseconds(-1 * kExampleTime.count());
    ASSERT_EQ("-1m 15s", HumanReadable::timeSpan(negativeMs,
        kFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST(HumanReadableTest, smallTimeSpanNegative)
{
    /* Check if small negative value correctly rounded to non-signed zero */
    const auto negativeMs = milliseconds(-1);
    ASSERT_EQ("0s", HumanReadable::timeSpan(negativeMs,
        kFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}


} // namespace core
} // namespace client
} // namespace nx
