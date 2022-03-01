// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_model.h"

#include <QtQml/QtQml>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <functional>

#include <nx/vms/client/core/time/calendar_utils.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/media/time_periods_store.h>

namespace {

struct Day
{
    Day();
    Day(const QDate& date, qint64 displayOffset);

    qint64 endTime() const;
    bool containsTime(qint64 value) const;

    qint64 startTime = 0;
    bool hasArchive = false;
    int dayNumber = 1;
};

Day::Day() {}

Day::Day(const QDate& date, qint64 displayOffset):
    startTime(QDateTime(date, QTime(), Qt::UTC).toMSecsSinceEpoch() - displayOffset),
    dayNumber(date.day())
{
}

qint64 Day::endTime() const
{
    static constexpr qint64 kMillisecondsInDay = 24 * 60 * 60 * 1000;
    return startTime + kMillisecondsInDay - 1;
}

bool Day::containsTime(qint64 value) const
{
    return startTime <= value && value <= endTime();
}

//--------------------------------------------------------------------------------------------------

struct Month
{
    Month(int year = 1970, int month = 1, qint64 displayOffset = 0);

    void recalculateData(qint64 displayOffset);
    bool containsDay(const Day& day) const;
    bool containsPosition(qint64 position) const;

    int year;
    int month;
    Day startDay;
    Day endDay;
};

Month::Month(int year, int month, qint64 displayOffset):
    year(year),
    month(month),
    startDay(QDate(year, month, 1), displayOffset),
    endDay(QDate(year, month, QDate(year, month, 1).daysInMonth()), displayOffset)
{
}

void Month::recalculateData(qint64 displayOffset)
{
    *this = Month(year, month, displayOffset);
}

bool Month::containsDay(const Day& day) const
{
    return day.startTime <= endDay.endTime() && day.endTime() >= startDay.startTime;
}

bool Month::containsPosition(qint64 position) const
{
    return startDay.startTime <= position && position <= endDay.endTime();
}

} // namespace

//--------------------------------------------------------------------------------------------------

namespace nx::vms::client::core {

struct CalendarModel::Private
{
    Private(CalendarModel* owner);
    void resetDaysModelData();
    void updateArchiveInfo();
    void handleCurrentPositionChanged();
    void clearArchiveMarks(int dayIndex = 0);

    CalendarModel* const q;

    qint64 currentPosition = 0;
    qint64 displayOffset = 0;
    Month currentMonth;
    QList<Day> days;
    QLocale locale;

    TimePeriodsStore* periodsStore = nullptr;
    bool populated = false;
};

CalendarModel::Private::Private(CalendarModel* owner):
    q(owner)
{
}

void CalendarModel::Private::resetDaysModelData()
{
    q->beginResetModel();

    days.clear();

    auto date = CalendarUtils::firstWeekStartDate(locale, currentMonth.year, currentMonth.month);
    if (!date.isValid())
        return;

    // First row will contain the first day of this month.
    static constexpr int kDaysInWeek = 7;
    for (int i = 0; i < kDaysInWeek; ++i)
    {
        days.append(Day(date, displayOffset));
        date = date.addDays(1);
    }

    // Add weeks till month ended.
    while (date.month() == currentMonth.month)
    {
        days.append(Day(date, displayOffset));
        date = date.addDays(1);
    }

    updateArchiveInfo();
    q->endResetModel();
    currentMonth.recalculateData(displayOffset);
}

void CalendarModel::Private::updateArchiveInfo()
{
    if (!periodsStore)
    {
        clearArchiveMarks();
        return;
    }

    const auto timePeriods = periodsStore->periods(Qn::RecordingContent);

    const int firstMonthDayPosition =
        [this]() -> int
        {
            const auto it = std::find_if(days.begin(), days.end(),
                [this](const Day& day) { return currentMonth.containsDay(day); });
            return it == days.end() ? -1 : it - days.begin();
        }();

    for (int i = firstMonthDayPosition; i != days.size();)
    {
        const auto it = timePeriods.findNearestPeriod(days[i].startTime, true);
        if (it == timePeriods.cend())
        {
            // No chinks at the right of the first day of month.
            clearArchiveMarks(i);
            break;
        }

        const auto chunkStartTime = it->startTimeMs;
        const auto chunkEndTime = it->isInfinite()
            ? QDateTime::currentMSecsSinceEpoch()
            : it->endTimeMs();

        while (i < days.size() && days[i].endTime() < chunkStartTime)
            days[i++].hasArchive = false;

        while (i < days.size() && days[i].startTime <= chunkEndTime)
            days[i++].hasArchive = true;

        if (it->isInfinite())
        {
            clearArchiveMarks(i);
            break;
        }
    }
}

void CalendarModel::Private::handleCurrentPositionChanged()
{
    emit q->dataChanged(q->index(0), q->index(q->rowCount() - 1), {CalendarModel::IsCurrentRole});
}

void CalendarModel::Private::clearArchiveMarks(int dayIndex)
{
    for (int i = dayIndex; i != days.size(); ++i)
        days[i].hasArchive = false;
}

//--------------------------------------------------------------------------------------------------

void CalendarModel::registerQmlType()
{
    qmlRegisterType<nx::vms::client::core::CalendarModel>("Nx.Core", 1, 0, "CalendarModel");
}

CalendarModel::CalendarModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    d->resetDaysModelData();
}

CalendarModel::~CalendarModel()
{
}

int CalendarModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->days.size();
}

QVariant CalendarModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const auto& day = d->days.at(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
            return d->currentMonth.containsDay(day) ? QString::number(day.dayNumber) : QString();
        case DayStartTimeRole:
            return day.startTime;
        case IsCurrentRole:
            return d->currentMonth.containsPosition(d->currentPosition)
                && day.containsTime(d->currentPosition);
        case HasArchiveRole:
            return day.hasArchive;
    }

    return QVariant();
}

QHash<int, QByteArray> CalendarModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[DayStartTimeRole] = "dayStartTime";
    roleNames[IsCurrentRole] = "isCurrent";
    roleNames[HasArchiveRole] = "hasArchive";
    return roleNames;
}

int CalendarModel::year() const
{
    return d->currentMonth.year;
}

void CalendarModel::setYear(int year)
{
    year = std::clamp(year, CalendarUtils::kMinYear, CalendarUtils::kMaxYear);
    if (year == d->currentMonth.year)
        return;

    d->currentMonth.year = year;
    emit yearChanged();

    d->resetDaysModelData();
}

int CalendarModel::month() const
{
    return d->currentMonth.month;
}

void CalendarModel::setMonth(int month)
{
    month = std::clamp(month, CalendarUtils::kMinMonth, CalendarUtils::kMaxMonth);
    if (month == d->currentMonth.month)
        return;

    d->currentMonth.month = month;
    emit yearChanged();

    d->resetDaysModelData();
}


TimePeriodsStore* CalendarModel::periodsStore() const
{
    return d->periodsStore;
}

void CalendarModel::setPeriodsStore(TimePeriodsStore* store)
{
    if (d->periodsStore == store)
        return;

    if (d->periodsStore)
        disconnect(d->periodsStore, nullptr, this, nullptr);

    d->periodsStore = store;

    const auto updateArchiveInfoInternal =
        [this](Qn::TimePeriodContent type)
        {
            if (type != Qn::RecordingContent)
                return;
            d->updateArchiveInfo();
            emit dataChanged(index(0), index(rowCount() - 1), {HasArchiveRole});
        };

    if (d->periodsStore)
    {
        connect(d->periodsStore, &TimePeriodsStore::periodsUpdated,
            this, updateArchiveInfoInternal);
    }

    updateArchiveInfoInternal(Qn::RecordingContent);
    emit periodsStoreChanged();
}

qint64 CalendarModel::position() const
{
    return d->currentPosition;
}

void CalendarModel::setPosition(qint64 value)
{
    value = std::clamp<qint64>(value, 0, std::numeric_limits<qint64>().max());
    if (d->currentPosition == value)
        return;

    d->currentPosition = value;
    emit positionChanged();

    d->handleCurrentPositionChanged();
}

qint64 CalendarModel::displayOffset() const
{
    return d->displayOffset;
}

void CalendarModel::setDisplayOffset(qint64 value)
{
    value = std::clamp<qint64>(
        value, CalendarUtils::kMinDisplayOffset, CalendarUtils::kMaxDisplayOffset);

    if (d->displayOffset == value)
        return;

    d->displayOffset = value;
    emit displayOffsetChanged();

    d->resetDaysModelData();
}

QLocale CalendarModel::locale() const
{
    return d->locale;
}

void CalendarModel::setLocale(const QLocale& locale)
{
    if (d->locale == locale)
        return;

    d->locale = locale;
    emit localeChanged();

    d->resetDaysModelData();
}

} // namespace nx::vms::client::core
