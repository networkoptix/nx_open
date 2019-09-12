#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/client/core/time/calendar_utils.h>
#include <nx/client/core/time/calendar_model.h>
#include <nx/client/core/time/time_common_properties_test.h>

namespace nx::client::core {
namespace test {

static constexpr int kTestYear = 2019;
static constexpr int kTestMonth = 9;
static constexpr qint64 kTestMonthStartUtc = 1567296000000;
static constexpr qint64 kTestMonthEndUtc = 1569887999999;

struct CalendarModelTestFixture: public testing::Test
{
    CalendarModel model;
};

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

struct CalendarModelBasicParams
{
    QLocale locale;
    int year;
    int month;
};

template <typename Base>
struct CalendarModelTimeTestFixture: public Base
{
    virtual void SetUp() override;

    CalendarModel model;
};

template <typename Base>
void CalendarModelTimeTestFixture<Base>::SetUp()
{
    const auto parameters = Base::GetParam();
    model.setYear(parameters.year);
    model.setMonth(parameters.month);
    model.setLocale(parameters.locale);
}

//--------------------------------------------------------------------------------------------------

struct StartDayOffsetTestParams: public CalendarModelBasicParams
{
    int expectedOffsetValue;
    QString expectedDisplayValue;
};

using CalendarModelStartDayValueTestFixture =
    CalendarModelTimeTestFixture<testing::TestWithParam<StartDayOffsetTestParams>>;

TEST_P(CalendarModelStartDayValueTestFixture, StartDayOffsetTest)
{
    const auto parameters = GetParam();
    const auto dayNumber = model.index(parameters.expectedOffsetValue).data().toString();
    ASSERT_EQ(dayNumber, parameters.expectedDisplayValue);
}

static const QString kFirstMonthDayDisplayText = "1";
static const QLocale kSundayMonthStartLocale = QLocale(QLocale::English, QLocale::UnitedStates);
static const QLocale kMondayMonthStartLocale = QLocale(QLocale::Russian, QLocale::RussianFederation);

// 1 September 2019 is Sunday. Testing values checks if visual model has proper offset of the first
// month's day (accroding to the specified locale).
static const std::vector<StartDayOffsetTestParams> kStartDayTestData = {
    {kSundayMonthStartLocale, kTestYear, kTestMonth, 0, kFirstMonthDayDisplayText},
    {kMondayMonthStartLocale, kTestYear, kTestMonth, 6, kFirstMonthDayDisplayText} };

INSTANTIATE_TEST_CASE_P(CalendarModel, CalendarModelStartDayValueTestFixture,
    testing::ValuesIn(kStartDayTestData));

//--------------------------------------------------------------------------------------------------

static const QLocale kPositionTestLocale(QLocale::Russian, QLocale::RussianFederation);
static const QString kExpectedNoHighlighValue("no highlighted items");
static const QString kExpectedFirstDayOfMonth = "1";
static const QString kExpectedLastDayOfMonth = "30";

enum DisplayOffset
{
    utc = 0,
    specified = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(3)).count()
};

struct PositionHighlightTestParams: public CalendarModelBasicParams
{
    using base_type = CalendarModelBasicParams;

    PositionHighlightTestParams(
        qint64 position,
        DisplayOffset displayOffset,
        QString expectedHighlightedDayNumber);

    qint64 position;
    DisplayOffset displayOffset;
    QString expectedHighlightedDayNumber;
};

PositionHighlightTestParams::PositionHighlightTestParams(
    qint64 position,
    DisplayOffset displayOffset,
    QString expectedHighlightedDayNumber)
    :
    base_type{kPositionTestLocale, kTestYear, kTestMonth},
    position(position),
    displayOffset(displayOffset),
    expectedHighlightedDayNumber(expectedHighlightedDayNumber)

{
}

struct CalendarModelPositionHighlightTestFixture:
    public CalendarModelTimeTestFixture<testing::TestWithParam<PositionHighlightTestParams>>
{
    using base_type = CalendarModelTimeTestFixture<testing::TestWithParam<PositionHighlightTestParams>>;

    virtual void SetUp() override
    {
        base_type::SetUp();

        const auto params = base_type::GetParam();
        model.setPosition(params.position);
        model.setDisplayOffset(params.displayOffset);
    }
};

TEST_P(CalendarModelPositionHighlightTestFixture, PositionHighlightTest)
{
    const auto params = GetParam();

    QString highlightedDayNumber = kExpectedNoHighlighValue;
    for (int row = 0; row != model.rowCount(); ++row)
    {
        const auto index = model.index(row);
        if (model.data(index, CalendarModel::IsCurrentRole).toBool())
        {
            highlightedDayNumber = model.data(index, Qt::DisplayRole).toString();
            break;
        }
    }

    ASSERT_EQ(highlightedDayNumber, params.expectedHighlightedDayNumber);
}

static const std::vector<PositionHighlightTestParams> kPositionHighlightTestingData = {
    // Utc display offset tests
    PositionHighlightTestParams(
        kTestMonthStartUtc,
        DisplayOffset::utc,
        kExpectedFirstDayOfMonth),
    PositionHighlightTestParams(
        kTestMonthEndUtc,
        DisplayOffset::utc,
        kExpectedLastDayOfMonth),
    PositionHighlightTestParams(
        kTestMonthStartUtc - 1,
        DisplayOffset::utc,
        kExpectedNoHighlighValue),
    PositionHighlightTestParams(
        kTestMonthEndUtc + 1,
        DisplayOffset::utc,
        kExpectedNoHighlighValue),

    // Specified display offset tests
    PositionHighlightTestParams(
        kTestMonthStartUtc,
        DisplayOffset::specified,
        kExpectedFirstDayOfMonth),
    PositionHighlightTestParams(
        kTestMonthStartUtc - DisplayOffset::specified,
        DisplayOffset::specified,
        kExpectedFirstDayOfMonth),
    PositionHighlightTestParams(
        kTestMonthStartUtc - DisplayOffset::specified - 1,
        DisplayOffset::specified,
        kExpectedNoHighlighValue),
    PositionHighlightTestParams(
        kTestMonthEndUtc,
        DisplayOffset::specified,
        kExpectedNoHighlighValue),
    PositionHighlightTestParams(
        kTestMonthEndUtc - DisplayOffset::specified,
        DisplayOffset::specified,
        kExpectedLastDayOfMonth),
    PositionHighlightTestParams(
        kTestMonthEndUtc - DisplayOffset::specified + 1,
        DisplayOffset::specified,
        kExpectedNoHighlighValue),
    };

INSTANTIATE_TEST_CASE_P(CalendarModel, CalendarModelPositionHighlightTestFixture,
    testing::ValuesIn(kPositionHighlightTestingData));

// Future tests: (this comment will be removed)
//      check archive chunk presence

} // namespace test
} // namespace nx::client::core
