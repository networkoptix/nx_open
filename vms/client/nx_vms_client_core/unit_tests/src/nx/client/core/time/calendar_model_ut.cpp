#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/client/core/time/time_constants.h>
#include <nx/client/core/time/calendar_model.h>

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
    const auto model = createModel();

    // Display offset related checks.
    // Checks if display offset setter/getter works correctly.
    model->setDisplayOffset(TimeConstants::kMinDisplayOffset);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, model->displayOffset());

    // Checks if minimum display offset value is constrained.
    model->setDisplayOffset(TimeConstants::kMinDisplayOffset - 1);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, model->displayOffset());

    // Checks if maximum display offset value is constrained.
    model->setDisplayOffset(TimeConstants::kMaxDisplayOffset + 1);
    ASSERT_EQ(TimeConstants::kMaxDisplayOffset, model->displayOffset());
}

TEST(CalendarModelTest, PositionPropertyCheck)
{
    const auto model = createModel();

    // Checks if current position setter/getter works correctly.
    static const int kSomePosition = 1000;
    model->setCurrentPosition(kSomePosition);
    ASSERT_EQ(kSomePosition, model->currentPosition());

    // Checks if minimum position value is constrained.
    model->setCurrentPosition(-1);
    ASSERT_EQ(model->currentPosition(), 0);
}
