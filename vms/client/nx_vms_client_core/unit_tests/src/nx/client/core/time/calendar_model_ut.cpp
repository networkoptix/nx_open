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

//--------------------------------------------------------------------------------------------------

struct StartDayOffsetTestParams
{
    QLocale locale;
    int year;
    int month;
    int startDayOffsetValue;
};

struct CalendarModelStartDayValueTestFixture: public testing::TestWithParam<StartDayOffsetTestParams>
{
    virtual void SetUp() override;

    CalendarModel model;
};

void CalendarModelStartDayValueTestFixture::SetUp()
{
    const auto parameters = GetParam();
    model.setYear(parameters.year);
    model.setMonth(parameters.month);
    model.setLocale(parameters.locale);
}

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
    const auto dayNumber = model.index(parameters.startDayOffsetValue).data().toString();
    ASSERT_EQ(dayNumber, kFirstDayText);
}

// 1 September 2019 is Sunday. Testing values checks if visual model has proper offset of the first
// month's day (accroding to the specified locale).
static std::vector<StartDayOffsetTestParams> kStartDayTestingValues = {
    {QLocale(QLocale::English, QLocale::UnitedStates), 2019, 9, 0},
    {QLocale(QLocale::Russian, QLocale::RussianFederation), 2019, 9, 6}};

INSTANTIATE_TEST_CASE_P(CalendarModel, CalendarModelStartDayValueTestFixture,
    testing::ValuesIn(kStartDayTestingValues));

} // namespace test
} // namespace nx::client::core
