// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/vms/client/core/time/calendar_utils.h>
#include <nx/vms/client/core/time/calendar_model.h>
#include <nx/vms/client/core/time/time_common_properties_test.h>
#include <nx/vms/client/core/media/time_periods_store.h>

namespace nx::vms::client::core {
namespace test {
namespace {

static const QLocale kTestLocale(QLocale::Russian, QLocale::RussianFederation);
static constexpr int kTestYear = 2019;
static constexpr int kTestMonth = 9;
static constexpr qint64 kTestMonthStartUtc = 1567296000000;
static constexpr qint64 kTestMonthEndUtc = 1569887999999;
static const QString kExpectedNoItem("no item");
static const QString kExpectedFirstDayOfMonth("1");
static const QString kExpectedLastDayOfMonth("30");

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
INSTANTIATE_TYPED_TEST_SUITE_P(CalendarModelTest, CommonTimePropertiesTest, TestingType);

//--------------------------------------------------------------------------------------------------

struct When
{
    QLocale locale = kTestLocale;
    int year = kTestYear;
    int month = kTestMonth;
};

template <typename Base>
struct TimeTestFixture: public Base
{
    virtual void SetUp() override;

    CalendarModel model;
};

template <typename Base>
void TimeTestFixture<Base>::SetUp()
{
    const auto params = Base::GetParam();
    model.setYear(params.when.year);
    model.setMonth(params.when.month);
    model.setLocale(params.when.locale);
}

//--------------------------------------------------------------------------------------------------

struct Expect
{
    int offsetValue;
    QString displayValue;
};

struct StartDayOffsetTestParams
{
    When when;
    Expect expected;
};

using CalendarModelStartDayValueTestFixture =
    TimeTestFixture<testing::TestWithParam<StartDayOffsetTestParams>>;

TEST_P(CalendarModelStartDayValueTestFixture, StartDayOffsetTest)
{
    const auto params = GetParam();
    const auto dayNumber = model.index(params.expected.offsetValue).data().toString();
    ASSERT_EQ(dayNumber, params.expected.displayValue);
}

static const QLocale kSundayMonthStartLocale = QLocale(QLocale::English, QLocale::UnitedStates);
static const QLocale kMondayMonthStartLocale = QLocale(QLocale::Russian, QLocale::RussianFederation);

// 1 September 2019 is Sunday. Testing values checks if visual model has proper offset of the first
// month's day (accroding to the specified locale).
static const std::vector<StartDayOffsetTestParams> kStartDayTestData =
{
    {
        When{kSundayMonthStartLocale, kTestYear, kTestMonth},
        Expect{0, kExpectedFirstDayOfMonth}
    },

    {
        When{kMondayMonthStartLocale, kTestYear, kTestMonth},
        Expect{6, kExpectedFirstDayOfMonth}
    }
};

INSTANTIATE_TEST_SUITE_P(CalendarModel, CalendarModelStartDayValueTestFixture,
    testing::ValuesIn(kStartDayTestData));

//--------------------------------------------------------------------------------------------------

enum DisplayOffset
{
    utc = 0,
    positive = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(3)).count(),
    negative = -positive
};

struct DisplayOffsetParams
{
    static const std::vector<DisplayOffsetParams> standardDisplayOffsets;

    DisplayOffsetParams(DisplayOffset displayOffset);

    When when;
    DisplayOffset displayOffset;
};

const std::vector<DisplayOffsetParams> DisplayOffsetParams::standardDisplayOffsets =
    {DisplayOffset::utc, DisplayOffset::positive, DisplayOffset::negative};

DisplayOffsetParams::DisplayOffsetParams(DisplayOffset displayOffset):
    displayOffset(displayOffset)
{
}

//--------------------------------------------------------------------------------------------------

struct HighlightTestFixture: public TimeTestFixture<testing::TestWithParam<DisplayOffsetParams>>
{
    using base_type = TimeTestFixture<testing::TestWithParam<DisplayOffsetParams>>;

    virtual void SetUp() override;
    QString findHighlightedRow(int role) const;

    qint64 firstDayStartTimestamp() const;
    qint64 lastDayEndTimestamp() const;
    qint64 beforeFirstDayStartTimestamp();
    qint64 afterLastDayStartTimestamp();
};

void HighlightTestFixture::SetUp()
{
    base_type::SetUp();

    const auto params = base_type::GetParam();
    model.setDisplayOffset(params.displayOffset);
}

QString HighlightTestFixture::findHighlightedRow(int role) const
{
    for (int row = 0; row != model.rowCount(); ++row)
    {
        const auto index = model.index(row);
        if (model.data(index, role).toBool())
            return model.data(index, Qt::DisplayRole).toString();
    }
    return kExpectedNoItem;
}

qint64 HighlightTestFixture::firstDayStartTimestamp() const
{
    return kTestMonthStartUtc - model.displayOffset();
}

qint64 HighlightTestFixture::lastDayEndTimestamp() const
{
    return kTestMonthEndUtc - model.displayOffset();
}

qint64 HighlightTestFixture::beforeFirstDayStartTimestamp()
{
    return kTestMonthStartUtc - model.displayOffset() - 1;
}

qint64 HighlightTestFixture::afterLastDayStartTimestamp()
{
    return kTestMonthEndUtc - model.displayOffset() + 1;
}

//--------------------------------------------------------------------------------------------------

using PositionHighlightTestFixture = HighlightTestFixture;

TEST_P(PositionHighlightTestFixture, MonthStartPositionHitTest)
{
    // If current position points to the start of the month -
    // we expect the first day in highlighted state.
    model.setPosition(firstDayStartTimestamp());
    ASSERT_EQ(findHighlightedRow(CalendarModel::IsCurrentRole), kExpectedFirstDayOfMonth);
}

TEST_P(PositionHighlightTestFixture, MonthEndPositionHitTest)
{
    // If current position points to the end of the month -
    // we expect the last day in highlighted state.
    model.setPosition(lastDayEndTimestamp());
    ASSERT_EQ(findHighlightedRow(CalendarModel::IsCurrentRole), kExpectedLastDayOfMonth);
}

TEST_P(PositionHighlightTestFixture, MonthStartPositionMissTest)
{
    // If current position is before the start of the month - we expect nothing highlighted.
    model.setPosition(beforeFirstDayStartTimestamp());
    ASSERT_EQ(findHighlightedRow(CalendarModel::IsCurrentRole), kExpectedNoItem);
}

TEST_P(PositionHighlightTestFixture, MonthEndPositionMissTest)
{
    // If current position is after the end of the month - we expect nothing highlighted.
    model.setPosition(afterLastDayStartTimestamp());
    ASSERT_EQ(findHighlightedRow(CalendarModel::IsCurrentRole), kExpectedNoItem);
}

INSTANTIATE_TEST_SUITE_P(CalendarModel, PositionHighlightTestFixture,
    testing::ValuesIn(DisplayOffsetParams::standardDisplayOffsets));

//--------------------------------------------------------------------------------------------------

enum ChunkDuration
{
    millisecond = 1,
    milliseconds = 2
};

struct ChunkHighlightTestFixture: public HighlightTestFixture
{
    using base_type = HighlightTestFixture;

    virtual void SetUp() override;
    void setSingleRecordingPeriod(qint64 position, qint64 duration);
    TimePeriodsStore periodsStore;
};

void ChunkHighlightTestFixture::SetUp()
{
    base_type::SetUp();
    model.setPeriodsStore(&periodsStore);
}

void ChunkHighlightTestFixture::setSingleRecordingPeriod(qint64 position, qint64 duration)
{
    periodsStore.setPeriods(Qn::RecordingContent, {QnTimePeriod(position, duration)});
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsInsideFirstDayTest)
{
    // If chunk starts at the first day of month - we expect the first day in highlighted state.
    setSingleRecordingPeriod(firstDayStartTimestamp(), ChunkDuration::millisecond);
    ASSERT_EQ(findHighlightedRow(CalendarModel::HasArchiveRole), kExpectedFirstDayOfMonth);
}

TEST_P(ChunkHighlightTestFixture, ChunkHoversFirstDayTest)
{
    // If chunk hovers the first day - we expect the first day in highlighted state.
    setSingleRecordingPeriod(beforeFirstDayStartTimestamp(), ChunkDuration::milliseconds);
    ASSERT_EQ(findHighlightedRow(CalendarModel::HasArchiveRole), kExpectedFirstDayOfMonth);
}

TEST_P(ChunkHighlightTestFixture, ChunkEndsBeforeFirstDayTest)
{
    // If chunk ends before the first day - we expect nothing highlighted
    setSingleRecordingPeriod(beforeFirstDayStartTimestamp(), ChunkDuration::millisecond);
    ASSERT_EQ(findHighlightedRow(CalendarModel::HasArchiveRole), kExpectedNoItem);
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsAtLastDayTest)
{
    // If chunk starts at the end of the last day of month -
    // we expect the last day in highlighted state.
    setSingleRecordingPeriod(lastDayEndTimestamp(), ChunkDuration::millisecond);
    ASSERT_EQ(findHighlightedRow(CalendarModel::HasArchiveRole), kExpectedLastDayOfMonth);
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsAfterLastDayTest)
{
    // If chunk starts after the last day end - we expect nothing highlighted
    setSingleRecordingPeriod(afterLastDayStartTimestamp(), ChunkDuration::millisecond);
    ASSERT_EQ(findHighlightedRow(CalendarModel::HasArchiveRole), kExpectedNoItem);
}

INSTANTIATE_TEST_SUITE_P(CalendarModel, ChunkHighlightTestFixture,
    testing::ValuesIn(DisplayOffsetParams::standardDisplayOffsets));

} // namespace
} // namespace test
} // namespace nx::vms::client::core
