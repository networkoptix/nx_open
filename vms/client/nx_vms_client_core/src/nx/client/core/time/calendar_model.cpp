#include "calendar_model.h"

#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <functional>

#include <nx/client/core/media/chunk_provider.h>
#include <nx/client/core/media/time_periods_store.h>

namespace {

QDate createDate(int year, int month)
{
    return QDate(year, month, 1);
}

//--------------------------------------------------------------------------------------------------

struct Day
{
    Day();
    Day(const QDate& date, qint64 displayOffset);

    qint64 endTime() const;
    bool containsTime(qint64 value) const;

    qint64 startTime = 0;
    bool hasArchive = false;
    int dayNumber = 1; //< todo: remove me
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
    void updateMonthTo(int year, int month, qint64 displayOffset);
    bool containsDay(const Day& day) const;

    int year = 1970;
    int month = 1;
    Day startDay;
    Day endDay;
};

void Month::updateMonthTo(int yearValue, int monthValue, qint64 displayOffset)
{
    year = yearValue;
    month = monthValue;
    const auto startDate = createDate(year, month);
    startDay = Day(startDate, displayOffset);
    endDay = Day(QDate(year, month, startDate.daysInMonth()), displayOffset);
}

bool Month::containsDay(const Day& day) const
{
    return day.startTime <= endDay.endTime() && day.endTime() >= startDay.startTime;
}

} // namespace

//--------------------------------------------------------------------------------------------------

namespace nx::client::core {

struct CalendarModel::Private
{
    Private(CalendarModel* owner);
    QDate getFirstCalendarDate(int year, int month) const;
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

QDate CalendarModel::Private::getFirstCalendarDate(int year, int month) const
{
    auto date = createDate(year, month);
    int dayOfWeek = date.dayOfWeek();
    if (locale.firstDayOfWeek() == Qt::Monday)
        date = date.addDays(1 - dayOfWeek);
    else if (dayOfWeek < Qt::Sunday)
        date = date.addDays(-dayOfWeek);
    return date;
}

void CalendarModel::Private::resetDaysModelData()
{
    q->beginResetModel();

    days.clear();

    auto date = getFirstCalendarDate(currentMonth.year, currentMonth.month);
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
        if (it == timePeriods.constEnd())
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

CalendarModel::CalendarModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
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
            return day.containsTime(d->currentPosition);
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
    if (year == d->currentMonth.year)
        return;

    d->currentMonth.updateMonthTo(year, d->currentMonth.month, d->displayOffset);
    emit yearChanged();

    d->resetDaysModelData();
}

int CalendarModel::month() const
{
    return d->currentMonth.month;
}

void CalendarModel::setMonth(int month)
{
    if (month == d->currentMonth.month)
        return;

    d->currentMonth.updateMonthTo(d->currentMonth.year, month, d->displayOffset);
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

qint64 CalendarModel::currentPosition() const
{
    return d->currentPosition;
}

void CalendarModel::setCurrentPosition(qint64 value)
{
    if (d->currentPosition == value)
        return;

    d->currentPosition = value;
    emit currentPositionChanged();

    d->handleCurrentPositionChanged();
}

qint64 CalendarModel::displayOffset() const
{
    return d->displayOffset;
}

void CalendarModel::setDisplayOffset(qint64 value)
{
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

} // namespace nx::client::core
