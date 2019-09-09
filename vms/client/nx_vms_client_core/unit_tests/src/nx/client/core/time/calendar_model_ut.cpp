#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/client/core/time/time_constants.h>
#include <nx/client/core/time/calendar_model.h>
#include <nx/client/core/time/time_common_properties_test_helpers.h>

namespace {

using TimeConstants = nx::client::core::TimeConstants;
using Model = nx::client::core::CalendarModel;
using ModelPtr = QSharedPointer<Model>;

ModelPtr createModel()
{
    return ModelPtr(new Model());
}

} // namespace

TEST(CalendarModelTest, YearPropertyCheck)
{
    const auto model = createModel();

    // Checks if year setter/getter works correctly.
    model->setYear(TimeConstants::kMinYear);
    ASSERT_EQ(model->year(), TimeConstants::kMinYear);

    // Checks if minimum year value is constrained.
    model->setYear(TimeConstants::kMinYear - 1);
    ASSERT_EQ(model->year(), TimeConstants::kMinYear);

    // Checks if maximum year value is constrained.
    model->setYear(TimeConstants::kMaxYear + 1);
    ASSERT_EQ(model->year(), TimeConstants::kMaxYear);
}

TEST(CalendarModelTest, MonthPropertyCheck)
{
    const auto model = createModel();

    // Checks if month setter/getter works correctly.
    model->setMonth(1);
    ASSERT_EQ(model->month(), 1);

    // Checks if minimum month value is constrained.
    model->setMonth(TimeConstants::kMinMonth - 1);
    ASSERT_EQ(model->month(), TimeConstants::kMinMonth);

    // Checks if maximum month valueis constrained.
    model->setMonth(TimeConstants::kMaxMonth + 1);
    ASSERT_EQ(model->month(), TimeConstants::kMaxMonth);
}

TEST(CalendarModelTest, DisplayOffsetPropertyCheck)
{
    checkDisplayOffsetPropertyInteractionAndRanges(createModel().get());
}

TEST(CalendarModelTest, PositionPropertyCheck)
{
    checkPositionPropertyInteractionAndRanges(createModel().get());
}
