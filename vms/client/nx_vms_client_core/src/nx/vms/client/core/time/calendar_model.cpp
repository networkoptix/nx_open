// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_model.h"

#include <functional>

#include <QtCore/QLocale>
#include <QtCore/QTimeZone>
#include <QtQml/QtQml>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/core/time/calendar_utils.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

NX_REFLECTION_ENUM_CLASS(PeriodStorageType, currentCamera, allCameras, count)

int roleForPeriodStorageType(PeriodStorageType type)
{
    switch (type)
    {
        case PeriodStorageType::currentCamera:
            return CalendarModel::HasArchiveRole;
        case PeriodStorageType::allCameras:
            return CalendarModel::AnyCameraHasArchiveRole;
        default:
            NX_ASSERT(false, "Invalid period store type requested %1", type);
            return -1;
    }
}

struct Day
{
    Day(const QDate& date, const QTimeZone& timeZone);

    QDate date;
    qint64 startTime = 0;
    qint64 endTime = 0;

    bool hasArchive[(int) PeriodStorageType::count]{false, false};
};

Day::Day(const QDate& date, const QTimeZone& timeZone):
    date(date),
    startTime(date.startOfDay(timeZone).toMSecsSinceEpoch()),
    endTime(date.addDays(1).startOfDay(timeZone).toMSecsSinceEpoch())
{
}

//-------------------------------------------------------------------------------------------------

struct Month
{
    Month(const QTimeZone& timeZone = {QTimeZone::LocalTime}, int year = 1970, int month = 1);

    void recalculateData(const QTimeZone& timeZone);
    bool containsDay(const Day& day) const;

    int year;
    int month;
    Day startDay;
    Day endDay;
};

Month::Month(const QTimeZone& timeZone, int year, int month):
    year(year),
    month(month),
    startDay(QDate(year, month, 1), timeZone),
    endDay(QDate(year, month, QDate(year, month, 1).daysInMonth()), timeZone)
{
}

void Month::recalculateData(const QTimeZone& timeZone)
{
    *this = Month(timeZone, year, month);
}

bool Month::containsDay(const Day& day) const
{
    return day.startTime <= endDay.endTime && day.endTime >= startDay.startTime;
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct CalendarModel::Private
{
    Private(CalendarModel* owner);
    void resetDaysModelData();
    void updateArchiveInfo(PeriodStorageType type);
    void setPeriodStorage(AbstractTimePeriodStorage* store, PeriodStorageType type);

    CalendarModel* const q;

    Month currentMonth;
    QList<Day> days;
    QLocale locale;
    QTimeZone timeZone{QTimeZone::LocalTime};

    Qn::TimePeriodContent timePeriodType = Qn::RecordingContent;
    AbstractTimePeriodStorage* periodStorage[(int) PeriodStorageType::count]{nullptr, nullptr};
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

    auto date = calendar_utils::firstWeekStartDate(locale, currentMonth.year, currentMonth.month);
    if (!date.isValid())
        return;

    constexpr int kDaysInWeek = 7;
    constexpr int kDisplayWeeks = 6;

    for (int week = 0; week < kDisplayWeeks; ++week)
    {
        for (int day = 0; day < kDaysInWeek; ++day)
        {
            days.append(Day(date, timeZone));
            date = date.addDays(1);
        }
    }

    updateArchiveInfo(PeriodStorageType::currentCamera);
    updateArchiveInfo(PeriodStorageType::allCameras);
    q->endResetModel();
    currentMonth.recalculateData(timeZone);
}

void CalendarModel::Private::updateArchiveInfo(PeriodStorageType type)
{
    const auto store = periodStorage[(int) type];

    const QnTimePeriodList timePeriods =
        store ? store->periods(timePeriodType) : QnTimePeriodList();

    const qint64 startPosition = days.first().startTime;

    const auto getNextPosition =
        [this](milliseconds timestamp)
        {
            const auto timestampDate =
                QDateTime::fromMSecsSinceEpoch(timestamp.count(), timeZone).date();

            return std::chrono::milliseconds(
                timestampDate.addDays(1).startOfDay(timeZone).toMSecsSinceEpoch());
        };

    const QBitArray archivePresence = calendar_utils::buildArchivePresence(
        timePeriods,
        milliseconds(startPosition),
        days.size(),
        getNextPosition);

    for (int i = 0; i < days.size(); ++i)
        days[i].hasArchive[(int) type] = archivePresence[i];
}

void CalendarModel::Private::setPeriodStorage(
    AbstractTimePeriodStorage* store, PeriodStorageType type)
{
    const auto currentStore = periodStorage[(int) type];

    if (currentStore == store)
        return;

    if (currentStore)
        currentStore->disconnect(q);

    periodStorage[(int) type] = store;

    const auto updateArchiveInfoInternal =
        [this, type](Qn::TimePeriodContent contentType)
        {
            if (contentType != timePeriodType)
                return;

            updateArchiveInfo(type);
            emit q->dataChanged(
                q->index(0), q->index(q->rowCount() - 1), {roleForPeriodStorageType(type)});
        };

    if (store)
    {
        QObject::connect(
            store, &AbstractTimePeriodStorage::periodsUpdated, q, updateArchiveInfoInternal);
    }

    updateArchiveInfoInternal(timePeriodType);

    if (type == PeriodStorageType::allCameras)
        emit q->allCamerasPeriodStorageChanged();
    else
        emit q->periodStorageChanged();
}

//-------------------------------------------------------------------------------------------------

void CalendarModel::registerQmlType()
{
    qmlRegisterType<CalendarModel>("nx.vms.client.core", 1, 0, "CalendarModel");
}

CalendarModel::CalendarModel(QObject* parent):
    QAbstractListModel(parent),
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
            return d->currentMonth.containsDay(day) ? QString::number(day.date.day()) : QString();
        case DateRole:
            return day.date;
        case HasArchiveRole:
            return day.hasArchive[(int) PeriodStorageType::currentCamera];
        case AnyCameraHasArchiveRole:
            return day.hasArchive[(int) PeriodStorageType::allCameras];
        case TimeRangeRole:
            return QVariant::fromValue(QnTimePeriod::fromInterval(day.startTime, day.endTime));
        case IsFutureDateRole:
            return day.date > QDateTime::currentDateTimeUtc().toTimeZone(d->timeZone).date();
    }

    return QVariant();
}

QHash<int, QByteArray> CalendarModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty())
    {
        roleNames = QAbstractListModel::roleNames();
        roleNames.insert({
            {DateRole, "date"},
            {HasArchiveRole, "hasArchive"},
            {AnyCameraHasArchiveRole, "anyCameraHasArchive"},
            {TimeRangeRole, "timeRange"},
            {IsFutureDateRole, "isFutureDate"}
        });
    }
    return roleNames;
}

int CalendarModel::year() const
{
    return d->currentMonth.year;
}

void CalendarModel::setYear(int year)
{
    year = std::max(year, calendar_utils::kMinYear);
    if (year == d->currentMonth.year)
        return;

    d->currentMonth = Month(d->timeZone, year, d->currentMonth.month);
    emit yearChanged();

    d->resetDaysModelData();
}

int CalendarModel::month() const
{
    return d->currentMonth.month;
}

void CalendarModel::setMonth(int month)
{
    month = std::clamp(month, calendar_utils::kMinMonth, calendar_utils::kMaxMonth);
    if (month == d->currentMonth.month)
        return;

    d->currentMonth = Month(d->timeZone, d->currentMonth.year, month);
    emit yearChanged();

    d->resetDaysModelData();
}

Qn::TimePeriodContent CalendarModel::timePeriodType() const
{
    return d->timePeriodType;
}

void CalendarModel::setTimePeriodType(Qn::TimePeriodContent type)
{
    if (d->timePeriodType == type)
        return;

    d->timePeriodType = type;
    emit timePeriodTypeChanged();

    d->resetDaysModelData();
}

AbstractTimePeriodStorage* CalendarModel::periodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::currentCamera];
}

void CalendarModel::setPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::currentCamera);
}

AbstractTimePeriodStorage* CalendarModel::allCamerasPeriodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::allCameras];
}

void CalendarModel::setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::allCameras);
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

QTimeZone CalendarModel::timeZone() const
{
    return d->timeZone;
}

void CalendarModel::setTimeZone(const QTimeZone& timeZone)
{
    if (d->timeZone == timeZone)
        return;

    d->timeZone = timeZone;
    emit timeZoneChanged();

    d->resetDaysModelData();
}

} // namespace nx::vms::client::core
