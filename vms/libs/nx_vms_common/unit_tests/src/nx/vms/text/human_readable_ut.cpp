// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <nx/utils/i18n/translation_manager.h>
#include <nx/vms/text/human_readable.h>

namespace nx::vms::text {
namespace test {

using namespace std::chrono;

namespace {

static const QString kLocale = "en_US";
static const milliseconds kExampleTime = minutes(1) + seconds(15);
static const QString kSeparator(' ');
static const QString kDecimalSeparator('.');
static const auto kTimeFormat = HumanReadable::Seconds | HumanReadable::Minutes;

} // namespace

class HumanReadableTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_translationManager = std::make_unique<nx::i18n::TranslationManager>();
        m_translationManager->startLoadingTranslations();
        const bool loaded = m_translationManager->installTranslation(kLocale);
        ASSERT_TRUE(loaded);
    }

    virtual void TearDown() override
    {
        m_translationManager->stopLoadingTranslations();
        m_translationManager.reset();
    }

    std::unique_ptr<nx::i18n::TranslationManager> m_translationManager;
};

TEST_F(HumanReadableTest, timeSpan)
{
    ASSERT_EQ("1 minute, 15 seconds", HumanReadable::timeSpan(kExampleTime).toStdString());
}

TEST_F(HumanReadableTest, timeSpanNegative)
{
    const auto negativeMs = milliseconds(-1 * kExampleTime.count());
    ASSERT_EQ("-1 minute, 15 seconds", HumanReadable::timeSpan(negativeMs).toStdString());
}

TEST_F(HumanReadableTest, shortTimeSpan)
{
    ASSERT_EQ("1m 15s", HumanReadable::timeSpan(kExampleTime,
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST_F(HumanReadableTest, longTimeSpan)
{
    ASSERT_EQ("1 min 15 sec", HumanReadable::timeSpan(kExampleTime,
        kTimeFormat,
        HumanReadable::SuffixFormat::Long,
        kSeparator)
        .toStdString());
}

TEST_F(HumanReadableTest, shortTimeSpanNegative)
{
    const auto negativeMs = milliseconds(-1 * kExampleTime.count());
    ASSERT_EQ("-1m 15s", HumanReadable::timeSpan(negativeMs,
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST_F(HumanReadableTest, smallTimeSpanNegative)
{
    /* Check if small negative value correctly rounded to non-signed zero */
    const auto negativeMs = milliseconds(-1);
    ASSERT_EQ("0s", HumanReadable::timeSpan(negativeMs,
        kTimeFormat,
        HumanReadable::SuffixFormat::Short,
        kSeparator)
        .toStdString());
}

TEST_F(HumanReadableTest, digitalVolumeSizeFixed)
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

TEST_F(HumanReadableTest, digitalVolumeSizeFixedOverflow)
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

TEST_F(HumanReadableTest, digitalVolumeSizePrecise)
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

} // namespace test
} // namespace nx::vms::text
