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
static const QString kDecimalSeparator(L'.');
static const auto kTimeFormat = HumanReadable::Seconds | HumanReadable::Minutes;

} // namespace

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
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}


TEST(HumanReadableTest, longTimeSpan)
{
    ASSERT_EQ("1 min 15 sec", HumanReadable::timeSpan(kExampleTime,
        kTimeFormat,
        HumanReadable::SuffixFormat::Long,
        kSeparator)
        .toStdString());
}

TEST(HumanReadableTest, shortTimeSpanNegative)
{
    const auto negativeMs = milliseconds(-1 * kExampleTime.count());
    ASSERT_EQ("-1m 15s", HumanReadable::timeSpan(negativeMs,
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST(HumanReadableTest, smallTimeSpanNegative)
{
    /* Check if small negative value correctly rounded to non-signed zero */
    const auto negativeMs = milliseconds(-1);
    ASSERT_EQ("0s", HumanReadable::timeSpan(negativeMs,
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST(HumanReadableTest, digitalVolumeSizeFixed)
{
    const auto size = 1315333734400;
    ASSERT_EQ("1 TB 201 GB", HumanReadable::digitalSize(size,
        HumanReadable::VolumeSize,
        HumanReadable::DigitalSizeMultiplier::Binary,
        HumanReadable::SuffixFormat::Long,
        kSeparator,
        /*suppressSecondUnitLimit*/ 200)
        .toStdString());
}

TEST(HumanReadableTest, digitalVolumeSizeFixedOverflow)
{
    const auto size = 1315333734400;
    ASSERT_EQ("1315333734400 Bytes", HumanReadable::digitalSize(size,
        HumanReadable::Bytes,
        HumanReadable::DigitalSizeMultiplier::Binary,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        /*suppressSecondUnitLimit*/ 200)
        .toStdString());
}

TEST(HumanReadableTest, digitalVolumeSizePrecise)
{
    const auto size = 1315333734400;
    ASSERT_EQ("1.2 TB", HumanReadable::digitalSizePrecise(size,
        HumanReadable::VolumeSize,
        HumanReadable::DigitalSizeMultiplier::Binary,
        HumanReadable::SuffixFormat::Long,
        kDecimalSeparator,
        /*precision*/ 1)
        .toStdString());
}



} // namespace core
} // namespace client
} // namespace nx
