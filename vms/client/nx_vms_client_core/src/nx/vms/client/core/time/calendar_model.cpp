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
    Day();
    Day(const QDate& date, qint64 displayOffset);

    qint64 endTime() const;

    QDate date;
    qint64 startTime = 0;

    bool hasArchive[(int) PeriodStorageType::count]{false, false};
};

Day::Day() {}

Day::Day(const QDate& date, qint64 displayOffset):
    date(date),
    startTime(QDateTime(date, QTime()).toMSecsSinceEpoch() - displayOffset)
{
}

qint64 Day::endTime() const
{
    static constexpr qint64 kMillisecondsInDay = 24 * 60 * 60 * 1000;
    return startTime + kMillisecondsInDay - 1;
}

//-------------------------------------------------------------------------------------------------

struct Month
{
    Month(int year = 1970, int month = 1, qint64 displayOffset = 0);

    void recalculateData(qint64 displayOffset);
    bool containsDay(const Day& day) const;

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

} // namespace

//-------------------------------------------------------------------------------------------------

struct CalendarModel::Private
{
    Private(CalendarModel* owner);
    void resetDaysModelData();
    void updateArchiveInfo(PeriodStorageType type);
    void setPeriodStorage(AbstractTimePeriodStorage* store, PeriodStorageType type);

    CalendarModel* const q;

    qint64 displayOffset = 0;
    Month currentMonth;
    QList<Day> days;
    QLocale locale;
    QTimeZone timeZone{QTimeZone::LocalTime}; //< FIXME: #sivanov Implement timezone usage.

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
            days.append(Day(date, displayOffset));
            date = date.addDays(1);
        }
    }

    updateArchiveInfo(PeriodStorageType::currentCamera);
    updateArchiveInfo(PeriodStorageType::allCameras);
    q->endResetModel();
    currentMonth.recalculateData(displayOffset);
}

void CalendarModel::Private::updateArchiveInfo(PeriodStorageType type)
{
    const auto store = periodStorage[(int) type];

    const QnTimePeriodList timePeriods =
        store ? store->periods(Qn::RecordingContent) : QnTimePeriodList();

    const qint64 startPosition = days.first().date.startOfDay(timeZone).toMSecsSinceEpoch();

    const QBitArray archivePresence = calendar_utils::buildArchivePresence(
        timePeriods,
        milliseconds(startPosition),
        days.size(),
        [](milliseconds timestamp) -> milliseconds { return timestamp + 24h; });

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
            if (contentType != Qn::RecordingContent)
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

    updateArchiveInfoInternal(Qn::RecordingContent);

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
            return d->currentMonth.containsDay(day) ? QString::number(day.date.day()) : QString();
        case DateRole:
            return QDateTime(day.date, QTime());
        case HasArchiveRole:
            return day.hasArchive[(int) PeriodStorageType::currentCamera];
        case AnyCameraHasArchiveRole:
            return day.hasArchive[(int) PeriodStorageType::allCameras];
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

    d->currentMonth = Month(year, d->currentMonth.month, d->displayOffset);
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

    d->currentMonth = Month(d->currentMonth.year, month, d->displayOffset);
    emit yearChanged();

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

qint64 CalendarModel::displayOffset() const
{
    return d->displayOffset;
}

void CalendarModel::setDisplayOffset(qint64 value)
{
    value = std::clamp<qint64>(
        value, calendar_utils::kMinDisplayOffset, calendar_utils::kMaxDisplayOffset);

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
