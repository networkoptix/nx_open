// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <array>

#include <nx/vms/client/core/time/display_time_helper.h>
#include <nx/vms/client/core/time/time_common_properties_test.h>

using namespace std::chrono;

namespace nx::vms::client::core {
namespace test {
namespace {

using TestingType = testing::Types<DisplayTimeHelper>;
INSTANTIATE_TYPED_TEST_SUITE_P(DisplayTimeHelperTest, CommonTimePropertiesTest, TestingType);

//--------------------------------------------------------------------------------------------------

static const QString kNoNoonMark;

namespace display_offset
{
    constexpr milliseconds kUtc = 0ms;
    constexpr milliseconds kNinetySeconds = 90s;
    constexpr milliseconds kOneHourForward = 1h;
    constexpr milliseconds kThreeHoursBack = -3h;
}

struct When
{
    milliseconds displayOffset = display_offset::kUtc;
};

struct Hour
{
    QString value;
    QString noonMark;
};

struct Expect
{
    Hour hoursIn12hFormat;
    Hour hoursIn24hFormat;
    QString minutes;
    QString seconds;
    QString fullDate;
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
    static const qint64 kTestPosition =
        QDateTime(QDate(2019, 9, 1), QTime(0, 9, 9)).toMSecsSinceEpoch();

    const auto params = GetParam();
    helper.setLocale(kTestLocale);
    helper.setPosition(kTestPosition);
    helper.setDisplayOffset(params.when.displayOffset.count());
}

TEST_P(HelperTestFixture, TestVisualValues)
{
    const auto params = GetParam();

    const auto is24HoursTimeFormatValues = {true, false};
    for (const bool is24HoursTimeFormat: is24HoursTimeFormatValues)
    {
        helper.set24HoursTimeFormat(is24HoursTimeFormat);

        const auto hours = is24HoursTimeFormat
            ? params.expected.hoursIn24hFormat
            : params.expected.hoursIn12hFormat;
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
        When{display_offset::kUtc},
        Expect
        {
            .hoursIn12hFormat{"12", "AM"},
            .hoursIn24hFormat{"00", kNoNoonMark},
            .minutes{"09"},
            .seconds{"09"},
            .fullDate{"1 September 2019"}
        }
    },

    {
        When{display_offset::kNinetySeconds},
        Expect
        {
            .hoursIn12hFormat{"12", "AM"},
            .hoursIn24hFormat{"00", kNoNoonMark},
            .minutes{"10"},
            .seconds{"39"},
            .fullDate{"1 September 2019"}
        }
    },

    {
        When{display_offset::kOneHourForward},
        Expect
        {
            .hoursIn12hFormat{"1", "AM"},
            .hoursIn24hFormat{"01", kNoNoonMark},
            .minutes{"09"},
            .seconds{"09"},
            .fullDate{"1 September 2019"}
        }
    },

    {
        When{display_offset::kThreeHoursBack},
        Expect
        {
            .hoursIn12hFormat{"9", "PM"},
            .hoursIn24hFormat{"21", kNoNoonMark},
            .minutes{"09"},
            .seconds{"09"},
            .fullDate{"31 August 2019"}
        }
    }
};

INSTANTIATE_TEST_SUITE_P(DisplayTimeHelperTest, HelperTestFixture,
    testing::ValuesIn(kTestData));

} // namespace
} // namespace test
} // namespace nx::vms::client::core
