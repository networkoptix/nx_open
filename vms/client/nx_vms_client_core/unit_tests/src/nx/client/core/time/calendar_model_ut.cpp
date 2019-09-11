#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/client/core/time/calendar_utils.h>
#include <nx/client/core/time/calendar_model.h>
#include <nx/client/core/time/time_common_properties_test.h>

namespace nx::client::core {
namespace test {

struct CalendarModelTestFixture: public testing::Test
{
    CalendarModel model;
};

struct FirstDayTimeParams
{
    int year;
    int month;
    int startDayOffsetValue;
    QLocale locale;
};

struct CalendarModelStartDayValueTestFixture: public testing::TestWithParam<FirstDayTimeParams>
{
    CalendarModel model;
};

//--------------------------------------------------------------------------------------------------

TEST_F(CalendarModelTestFixture, YearPropertyCheck)
{
    // Checks if year setter/getter works correctly.
    model.setYear(CalendarUtils::kMinYear);
    ASSERT_EQ(model.year(), CalendarUtils::kMinYear);

    // Checks if minimum year value is constrained.
    model.setYear(CalendarUtils::kMinYear - 1);
    ASSERT_EQ(model.year(), CalendarUtils::kMinYear);

    // Checks if maximum year value is constrained.
    model.setYear(CalendarUtils::kMaxYear + 1);
    ASSERT_EQ(model.year(), CalendarUtils::kMaxYear);
}

TEST_F(CalendarModelTestFixture, MonthPropertyCheck)
{
    // Checks if month setter/getter works correctly.
    model.setMonth(1);
    ASSERT_EQ(model.month(), 1);

    // Checks if minimum month value is constrained.
    model.setMonth(CalendarUtils::kMinMonth - 1);
    ASSERT_EQ(model.month(), CalendarUtils::kMinMonth);

    // Checks if maximum month valueis constrained.
    model.setMonth(CalendarUtils::kMaxMonth + 1);
    ASSERT_EQ(model.month(), CalendarUtils::kMaxMonth);
}

using TestingType = testing::Types<CalendarModel>;
INSTANTIATE_TYPED_TEST_CASE_P(CalendarModelTest, CommonTimePropertiesTest, TestingType);

//--------------------------------------------------------------------------------------------------

// Future tests: (this comment will be removed)
//      +check start day of month offset (according to the us/eu week start day)
//      check if position pointing to the month beginning is displayed correctly in data
//      check if position pointing to the month ending is displayed correctly in data
//      check archive chunk presence

TEST_P(CalendarModelStartDayValueTestFixture, StartDayOffset)
{
    static const QString kFirstDayText = "1";

    const auto parameters = GetParam();
    model.setYear(parameters.year);
    model.setMonth(parameters.month);
    model.setLocale(parameters.locale);

    const auto dayNumber = model.index(parameters.startDayOffsetValue).data().toString();
    ASSERT_EQ(dayNumber, kFirstDayText);
}

static constexpr int kYear = 2019;
static constexpr int kMonth = 9;
static const FirstDayTimeParams kSundayWeekStartDayCheckParams =
    {kYear, kMonth, 0, QLocale(QLocale::English, QLocale::UnitedStates)};
static const FirstDayTimeParams kMondayWeekStartDayCheckParams =
    {kYear, kMonth, 6, QLocale(QLocale::Russian, QLocale::RussianFederation)};

INSTANTIATE_TEST_CASE_P(CalendarModel, CalendarModelStartDayValueTestFixture,
    testing::Values(kSundayWeekStartDayCheckParams, kMondayWeekStartDayCheckParams));

} // namespace test
} // namespace nx::client::core
