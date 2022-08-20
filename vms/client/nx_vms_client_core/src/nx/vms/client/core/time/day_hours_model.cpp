// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "day_hours_model.h"

#include <QtQml/QtQml>
#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <functional>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <recording/time_period_list.h>

#include "calendar_utils.h"

namespace nx::vms::client::core {

namespace {

constexpr int kHoursPerDay = 24;

NX_REFLECTION_ENUM_CLASS(PeriodStorageType, currentCamera, allCameras, count)

int roleForPeriodStorageType(PeriodStorageType type)
{
    switch (type)
    {
        case PeriodStorageType::currentCamera:
            return DayHoursModel::HasArchiveRole;
        case PeriodStorageType::allCameras:
            return DayHoursModel::AnyCameraHasArchiveRole;
        default:
            NX_ASSERT(false, "Invalid period store type requested %1", type);
            return -1;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct DayHoursModel::Private
{
    Private(DayHoursModel* owner);
    void resetModelData();
    void updateArchiveInfo(PeriodStorageType type);
    void clearArchiveMarks(PeriodStorageType type, int hour = 0);
    void setPeriodStorage(AbstractTimePeriodStorage* store, PeriodStorageType type);

    DayHoursModel* const q;

    bool amPmTime = true;
    QDate date;
    qint64 displayOffset = 0;
    bool hourHasArchive[(int) PeriodStorageType::count][kHoursPerDay];

    AbstractTimePeriodStorage* periodStorage[(int) PeriodStorageType::count]{nullptr, nullptr};
    bool populated = false;
};

DayHoursModel::Private::Private(DayHoursModel* owner):
    q(owner)
{
}

void DayHoursModel::Private::resetModelData()
{
    q->beginResetModel();
    updateArchiveInfo(PeriodStorageType::currentCamera);
    updateArchiveInfo(PeriodStorageType::allCameras);
    q->endResetModel();
}

void DayHoursModel::Private::updateArchiveInfo(PeriodStorageType type)
{
    const auto store = periodStorage[(int) type];
    if (!store)
    {
        clearArchiveMarks(type);
        return;
    }

    const QnTimePeriodList timePeriods = store->periods(Qn::RecordingContent);

    const qint64 startPosition = date.startOfDay().toMSecsSinceEpoch() - displayOffset;
    constexpr qint64 kMsPerHour = 60 * 60 * 1000;

    const auto hourStart = [startPosition](int hour) { return startPosition + hour * kMsPerHour; };
    const auto hourEnd =
        [startPosition](int hour) { return startPosition + (hour + 1) * kMsPerHour; };

    for (int hour = 0; hour < kHoursPerDay;)
    {
        const auto it = timePeriods.findNearestPeriod(hourStart(hour), true);
        if (it == timePeriods.cend())
        {
            clearArchiveMarks(type, hour);
            break;
        }

        const auto chunkStartTime = it->startTimeMs;
        const auto chunkEndTime = it->isInfinite()
            ? QDateTime::currentMSecsSinceEpoch()
            : it->endTimeMs();

        while (hour < kHoursPerDay && hourEnd(hour) < chunkStartTime)
            hourHasArchive[(int) type][hour++] = false;

        while (hour < kHoursPerDay && hourStart(hour) <= chunkEndTime)
            hourHasArchive[(int) type][hour++] = true;

        if (it->isInfinite())
        {
            clearArchiveMarks(type, hour);
            break;
        }
    }
}

void DayHoursModel::Private::clearArchiveMarks(PeriodStorageType type, int hour)
{
    for (int i = hour; i != kHoursPerDay; ++i)
        hourHasArchive[(int) type][i] = false;
}

void DayHoursModel::Private::setPeriodStorage(
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

DayHoursModel::DayHoursModel(QObject* parent):
    QAbstractListModel(parent),
    d(new Private(this))
{
    d->resetModelData();
}

DayHoursModel::~DayHoursModel()
{
}

bool DayHoursModel::amPmTime() const
{
    return d->amPmTime;
}

void DayHoursModel::setAmPmTime(bool amPmTime)
{
    if (d->amPmTime == amPmTime)
        return;

    d->amPmTime = amPmTime;
    emit amPmTimeChanged();
}

QDate DayHoursModel::date() const
{
    return d->date;
}

void DayHoursModel::setDate(const QDate& date)
{
    if (d->date == date)
        return;

    d->date = date;
    emit dateChanged();

    d->resetModelData();
}

AbstractTimePeriodStorage* DayHoursModel::periodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::currentCamera];
}

void DayHoursModel::setPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::currentCamera);
}

AbstractTimePeriodStorage* DayHoursModel::allCamerasPeriodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::allCameras];
}

void DayHoursModel::setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::allCameras);
}

qint64 DayHoursModel::displayOffset() const
{
    return d->displayOffset;
}

void DayHoursModel::setDisplayOffset(qint64 value)
{
    value = std::clamp<qint64>(
        value, CalendarUtils::kMinDisplayOffset, CalendarUtils::kMaxDisplayOffset);

    if (d->displayOffset == value)
        return;

    d->displayOffset = value;
    emit displayOffsetChanged();

    d->resetModelData();
}

void DayHoursModel::registerQmlType()
{
    qmlRegisterType<DayHoursModel>("nx.vms.client.core", 1, 0, "DayHoursModel");
}

int DayHoursModel::rowCount(const QModelIndex& /*parent*/) const
{
    return kHoursPerDay;
}

QVariant DayHoursModel::data(const QModelIndex& index, int role) const
{
    const auto hour = index.row();

    if (!hasIndex(hour, index.column(), index.parent()))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            if (!d->amPmTime)
                return hour;
            return (hour == 0 || hour == 12) ? 12 : hour % 12;
        case HourRole:
            return hour;
        case HasArchiveRole:
            return d->hourHasArchive[(int) PeriodStorageType::currentCamera][hour];
        case AnyCameraHasArchiveRole:
            return d->hourHasArchive[(int) PeriodStorageType::allCameras][hour];
    }

    return QVariant();
}

QHash<int, QByteArray> DayHoursModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty())
    {
        roleNames = QAbstractListModel::roleNames();
        roleNames.insert({
            {HourRole, "hour"},
            {HasArchiveRole, "hasArchive"},
            {AnyCameraHasArchiveRole, "anyCameraHasArchive"},
        });
    }
    return roleNames;
}

} // namespace nx::vms::client::core
