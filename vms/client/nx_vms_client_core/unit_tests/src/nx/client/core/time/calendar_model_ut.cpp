#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/client/core/time/time_constants.h>
#include <nx/client/core/time/calendar_model.h>
#include <nx/client/core/time/time_common_properties_test.h>

namespace nx::client::core {
namespace test {

struct CalendarModelTestFixture: public testing::Test
{
    CalendarModel model;
};

TEST_F(CalendarModelTestFixture, YearPropertyCheck)
{
    // Checks if year setter/getter works correctly.
    model.setYear(TimeConstants::kMinYear);
    ASSERT_EQ(model.year(), TimeConstants::kMinYear);

    // Checks if minimum year value is constrained.
    model.setYear(TimeConstants::kMinYear - 1);
    ASSERT_EQ(model.year(), TimeConstants::kMinYear);

    // Checks if maximum year value is constrained.
    model.setYear(TimeConstants::kMaxYear + 1);
    ASSERT_EQ(model.year(), TimeConstants::kMaxYear);
}

TEST_F(CalendarModelTestFixture, MonthPropertyCheck)
{
    // Checks if month setter/getter works correctly.
    model.setMonth(1);
    ASSERT_EQ(model.month(), 1);

    // Checks if minimum month value is constrained.
    model.setMonth(TimeConstants::kMinMonth - 1);
    ASSERT_EQ(model.month(), TimeConstants::kMinMonth);

    // Checks if maximum month valueis constrained.
    model.setMonth(TimeConstants::kMaxMonth + 1);
    ASSERT_EQ(model.month(), TimeConstants::kMaxMonth);
}

using TestingType = testing::Types<CalendarModel>;
INSTANTIATE_TYPED_TEST_CASE_P(CalendarModelTest, CommonTimePropertiesTest, TestingType);

} // namespace test
} // namespace nx::client::core
