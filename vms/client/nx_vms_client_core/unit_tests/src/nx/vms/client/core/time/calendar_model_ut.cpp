// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/core/time/calendar_model.h>
#include <nx/vms/client/core/time/calendar_utils.h>
#include <nx/vms/client/core/time/time_common_properties_test.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::core {
namespace test {
namespace {

static const QLocale kTestLocale(QLocale::Russian, QLocale::RussianFederation);
static constexpr int kTestYear = 2019;
static constexpr int kTestMonth = 9;
static const auto kTestMonthStart =
    QDateTime(QDate(kTestYear, kTestMonth, 1), QTime()); //< Sunday.
static const auto kFirstVisibleDay =
    kTestMonthStart.addDays(Qt::Monday - kTestMonthStart.date().dayOfWeek());
static const auto kTestMonthEnd = kTestMonthStart.addMonths(1).addMSecs(-1);
static const QString kExpectedFirstDayOfMonth("1");
static const QString kExpectedLastDayOfMonth("30");

struct CalendarModelTestFixture: public testing::Test
{
    CalendarModel model;
};

TEST_F(CalendarModelTestFixture, YearPropertyCheck)
{
    // Checks if year setter/getter works correctly.
    model.setYear(calendar_utils::kMinYear);
    ASSERT_EQ(model.year(), calendar_utils::kMinYear);

    // Checks if minimum year value is constrained.
    model.setYear(calendar_utils::kMinYear - 1);
    ASSERT_EQ(model.year(), calendar_utils::kMinYear);
}

TEST_F(CalendarModelTestFixture, MonthPropertyCheck)
{
    // Checks if month setter/getter works correctly.
    model.setMonth(1);
    ASSERT_EQ(model.month(), 1);

    // Checks if minimum month value is constrained.
    model.setMonth(calendar_utils::kMinMonth - 1);
    ASSERT_EQ(model.month(), calendar_utils::kMinMonth);

    // Checks if maximum month valueis constrained.
    model.setMonth(calendar_utils::kMaxMonth + 1);
    ASSERT_EQ(model.month(), calendar_utils::kMaxMonth);
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
    QDate findHighlightedCell() const;

    qint64 firstDayStartTimestamp() const;
    qint64 lastDayEndTimestamp() const;
    qint64 beforeFirstVisibleDayStartTimestamp();
    qint64 afterLastDayStartTimestamp();

    QVariant cellData(int index, int role) const
    {
        return model.data(model.index(index), role);
    }

    bool isCellHighlighted(int index) const
    {
        return cellData(index, CalendarModel::HasArchiveRole).toBool();
    }

    QDateTime cellDate(int index) const
    {
        return cellData(index, CalendarModel::DateRole).toDateTime();
    }
};

void HighlightTestFixture::SetUp()
{
    base_type::SetUp();

    const auto params = base_type::GetParam();
    model.setDisplayOffset(params.displayOffset);
}

QDate HighlightTestFixture::findHighlightedCell() const
{
    for (int index = 0; index != model.rowCount(); ++index)
    {
        const QDateTime date = cellDate(index);
        if (date <= kTestMonthEnd && isCellHighlighted(index))
            return date.date();
    }
    return {};
}

qint64 HighlightTestFixture::firstDayStartTimestamp() const
{
    auto testMonthStart = kTestMonthStart;
    testMonthStart.setTimeZone(model.timeZone());
    return testMonthStart.toMSecsSinceEpoch();
}

qint64 HighlightTestFixture::lastDayEndTimestamp() const
{
    auto testMonthEnd = kTestMonthEnd;
    testMonthEnd.setTimeZone(model.timeZone());
    return testMonthEnd.toMSecsSinceEpoch();
}

qint64 HighlightTestFixture::beforeFirstVisibleDayStartTimestamp()
{
    auto firstVisibleDay = kFirstVisibleDay;
    firstVisibleDay.setTimeZone(model.timeZone());
    return firstVisibleDay.toMSecsSinceEpoch() - 1;
}

qint64 HighlightTestFixture::afterLastDayStartTimestamp()
{
    auto testMonthEnd = kTestMonthEnd;
    testMonthEnd.setTimeZone(model.timeZone());
    return testMonthEnd.toMSecsSinceEpoch() + 1;
}

//--------------------------------------------------------------------------------------------------

enum ChunkDuration
{
    millisecond = 1,
    milliseconds = 2
};

class TimePeriodStorageMock: public AbstractTimePeriodStorage
{
public:
    virtual const QnTimePeriodList& periods(Qn::TimePeriodContent type) const override
    {
        return m_periods[type];
    }

    void setPeriods(Qn::TimePeriodContent type, QnTimePeriodList value)
    {
        m_periods[type] = std::move(value);
        emit periodsUpdated(type);
    }

private:
    std::array<QnTimePeriodList, Qn::TimePeriodContentCount> m_periods;
};

struct ChunkHighlightTestFixture: public HighlightTestFixture
{
    using base_type = HighlightTestFixture;

    virtual void SetUp() override;
    void setSingleRecordingPeriod(qint64 position, qint64 duration);
    TimePeriodStorageMock periodStorage;
};

void ChunkHighlightTestFixture::SetUp()
{
    base_type::SetUp();
    model.setPeriodStorage(&periodStorage);
}

void ChunkHighlightTestFixture::setSingleRecordingPeriod(qint64 position, qint64 duration)
{
    periodStorage.setPeriods(Qn::RecordingContent, {QnTimePeriod(position, duration)});
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsInsideFirstDayTest)
{
    // If chunk starts at the first day of month - we expect the first day in highlighted state.
    setSingleRecordingPeriod(firstDayStartTimestamp(), ChunkDuration::millisecond);
    EXPECT_EQ(findHighlightedCell(), kTestMonthStart.date());
}

TEST_P(ChunkHighlightTestFixture, ChunkHoversFirstVisibleDayTest)
{
    // If chunk hovers the first visible day, we expect the first day in highlighted state.
    setSingleRecordingPeriod(beforeFirstVisibleDayStartTimestamp(), ChunkDuration::milliseconds);
    EXPECT_TRUE(isCellHighlighted(0));
    EXPECT_FALSE(isCellHighlighted(1));
}

TEST_P(ChunkHighlightTestFixture, ChunkEndsBeforeFirstVisibleDayTest)
{
    // If chunk ends before the first day, we expect nothing is highlighted.
    setSingleRecordingPeriod(beforeFirstVisibleDayStartTimestamp(), ChunkDuration::millisecond);
    EXPECT_FALSE(isCellHighlighted(0));
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsAtLastDayTest)
{
    // If chunk starts at the end of the last day of month -
    // we expect the last day in highlighted state.
    setSingleRecordingPeriod(lastDayEndTimestamp(), ChunkDuration::millisecond);
    EXPECT_EQ(findHighlightedCell(), kTestMonthEnd.date());
}

TEST_P(ChunkHighlightTestFixture, ChunkStartsAfterLastDayTest)
{
    // If chunk starts after the last day end - we expect nothing highlighted
    setSingleRecordingPeriod(afterLastDayStartTimestamp(), ChunkDuration::millisecond);
    EXPECT_EQ(findHighlightedCell(), QDate());
}

INSTANTIATE_TEST_SUITE_P(CalendarModel, ChunkHighlightTestFixture,
    testing::ValuesIn(DisplayOffsetParams::standardDisplayOffsets));

} // namespace
} // namespace test
} // namespace nx::vms::client::core
