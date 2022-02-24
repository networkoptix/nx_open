// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <array>

#include <nx/vms/client/core/time/display_time_helper.h>
#include <nx/vms/client/core/time/time_common_properties_test.h>

namespace nx::vms::client::core {
namespace test {
namespace {

using TestingType = testing::Types<DisplayTimeHelper>;
INSTANTIATE_TYPED_TEST_SUITE_P(DisplayTimeHelperTest, CommonTimePropertiesTest, TestingType);

//--------------------------------------------------------------------------------------------------

static const QString kNoNoonMark;

enum DisplayOffset
{
    utc = 0,
    ninetySeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(90)).count(),
    oneHourForward = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(1)).count(),
    threeHoursBack = -std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(3)).count(),
};

struct When
{
    DisplayOffset displayOffset = DisplayOffset::utc;
};

struct Hour
{
    QString value;
    QString noonMark;
};

using HoursIn12Format = Hour;
using HoursIn24Format = Hour;
using Minutes = QString;
using Seconds = QString;
using FullDate = QString;

struct Expect
{
    std::array<Hour, 2> hours;
    Minutes minutes;
    Seconds seconds;
    FullDate fullDate;
};

struct HelperTestParams
{
    When when;
    Expect expected;
};

struct HelperTestFixture: public testing::TestWithParam<HelperTestParams>
{
    virtual void SetUp() override;

    DisplayTimeHelper helper;
};

void HelperTestFixture::SetUp()
{
    static const QLocale kTestLocale(QLocale::English, QLocale::UnitedStates);
    static constexpr qint64 kTestPosition = 1567296549000; //< 0:09:09, 1 September 2019 UTC

    const auto params = GetParam();
    helper.setLocale(kTestLocale);
    helper.setPosition(kTestPosition);
    helper.setDisplayOffset(params.when.displayOffset);
}

TEST_P(HelperTestFixture, TestVisualValues)
{
    const auto params = GetParam();

    const auto is24HoursTimeFormatValues = {true, false};
    for (const bool is24HoursTimeFormat: is24HoursTimeFormatValues)
    {
        helper.set24HoursTimeFormat(is24HoursTimeFormat);

        const auto hours = params.expected.hours[is24HoursTimeFormat];
        ASSERT_EQ(helper.hours(), hours.value);
        ASSERT_EQ(helper.noonMark(), hours.noonMark);
        ASSERT_EQ(helper.minutes(), params.expected.minutes);
        ASSERT_EQ(helper.seconds(), params.expected.seconds);
        ASSERT_EQ(helper.fullDate(), params.expected.fullDate);
    }
}

// Default position for tests is 0:09:09, 1 September 2019 UTC
static const std::vector<HelperTestParams> kTestData =
{
    {
        When{DisplayOffset::utc},
        Expect
        {
            {HoursIn12Format{"12", "AM"}, HoursIn24Format{"00", kNoNoonMark}},
            Minutes{"09"},
            Seconds{"09"},
            FullDate{"1 September 2019"}
        }
    },

    {
        When{DisplayOffset::ninetySeconds},
        Expect
        {
            {HoursIn12Format{"12", "AM"}, HoursIn24Format{"00", kNoNoonMark}},
            Minutes{"10"},
            Seconds{"39"},
            FullDate{"1 September 2019"}
        }
    },

    {
        When{DisplayOffset::oneHourForward},
        Expect
        {
            {HoursIn12Format{"1", "AM"}, HoursIn24Format{"01", kNoNoonMark}},
            Minutes{"09"},
            Seconds{"09"},
            FullDate{"1 September 2019"}
        }
    },

    {
        When{DisplayOffset::threeHoursBack},
        Expect
        {
            {HoursIn12Format{"9", "PM"}, HoursIn24Format{"21", kNoNoonMark}},
            Minutes{"09"},
            Seconds{"09"},
            FullDate{"31 August 2019"}
        }
    }
};

INSTANTIATE_TEST_SUITE_P(DisplayTimeHelperTest, HelperTestFixture,
    testing::ValuesIn(kTestData));

} // namespace
} // namespace test
} // namespace nx::vms::client::core
