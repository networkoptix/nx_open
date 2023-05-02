// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "month_list_model.h"

#include <QtQml/QtQml>
#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <recording/time_period_list.h>

#include "calendar_utils.h"

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

constexpr int kMonthsPerYear = 12;

NX_REFLECTION_ENUM_CLASS(PeriodStorageType, currentCamera, allCameras, count)

int roleForPeriodStorageType(PeriodStorageType type)
{
    switch (type)
    {
        case PeriodStorageType::currentCamera:
            return MonthListModel::HasArchiveRole;
        case PeriodStorageType::allCameras:
            return MonthListModel::AnyCameraHasArchiveRole;
        default:
            NX_ASSERT(false, "Invalid period store type requested %1", type);
            return -1;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct MonthListModel::Private
{
    Private(MonthListModel* owner);
    void resetModelData();
    void updateArchiveInfo(PeriodStorageType type);
    void setPeriodStorage(AbstractTimePeriodStorage* store, PeriodStorageType type);

    MonthListModel* const q;

    int year = calendar_utils::kMinYear;
    qint64 displayOffset = 0;
    QLocale locale;
    bool hourHasArchive[(int) PeriodStorageType::count][kMonthsPerYear];

    AbstractTimePeriodStorage* periodStorage[(int) PeriodStorageType::count]{nullptr, nullptr};
    bool populated = false;
};

MonthListModel::Private::Private(MonthListModel* owner):
    q(owner)
{
}

void MonthListModel::Private::resetModelData()
{
    q->beginResetModel();
    updateArchiveInfo(PeriodStorageType::currentCamera);
    updateArchiveInfo(PeriodStorageType::allCameras);
    q->endResetModel();
}

void MonthListModel::Private::updateArchiveInfo(PeriodStorageType type)
{
    const auto store = periodStorage[(int) type];

    const QnTimePeriodList timePeriods =
        store ? store->periods(Qn::RecordingContent) : QnTimePeriodList();

    const qint64 startPosition =
        QDate(year, 1, 1).startOfDay().toMSecsSinceEpoch() - displayOffset;

    const QBitArray archivePresence = calendar_utils::buildArchivePresence(
        timePeriods,
        milliseconds(startPosition),
        kMonthsPerYear,
        [](milliseconds timestamp)
        {
            return milliseconds(QDateTime::fromMSecsSinceEpoch(
                timestamp.count()).addMonths(1).toMSecsSinceEpoch());
        });

    for (int i = 0; i < kMonthsPerYear; ++i)
        hourHasArchive[(int) type][i] = archivePresence[i];
}

void MonthListModel::Private::setPeriodStorage(
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

MonthListModel::MonthListModel(QObject* parent):
    QAbstractListModel(parent),
    d(new Private(this))
{
    d->resetModelData();
}

MonthListModel::~MonthListModel()
{
}

int MonthListModel::year() const
{
    return d->year;
}

void MonthListModel::setYear(int year)
{
    if (d->year == year)
        return;

    d->year = year;
    emit yearChanged();

    d->resetModelData();
}

QLocale MonthListModel::locale() const
{
    return d->locale;
}

void MonthListModel::setLocale(const QLocale& locale)
{
    if (d->locale == locale)
        return;

    d->locale = locale;
    emit localeChanged();
    emit dataChanged(index(0), index(rowCount() - 1), {Qt::DisplayRole});
}

AbstractTimePeriodStorage* MonthListModel::periodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::currentCamera];
}

void MonthListModel::setPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::currentCamera);
}

AbstractTimePeriodStorage* MonthListModel::allCamerasPeriodStorage() const
{
    return d->periodStorage[(int) PeriodStorageType::allCameras];
}

void MonthListModel::setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store)
{
    d->setPeriodStorage(store, PeriodStorageType::allCameras);
}

qint64 MonthListModel::displayOffset() const
{
    return d->displayOffset;
}

void MonthListModel::setDisplayOffset(qint64 value)
{
    value = std::clamp<qint64>(
        value, calendar_utils::kMinDisplayOffset, calendar_utils::kMaxDisplayOffset);

    if (d->displayOffset == value)
        return;

    d->displayOffset = value;
    emit displayOffsetChanged();

    d->resetModelData();
}

void MonthListModel::registerQmlType()
{
    qmlRegisterType<MonthListModel>("nx.vms.client.core", 1, 0, "MonthListModel");
}

int MonthListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return kMonthsPerYear;
}

QVariant MonthListModel::data(const QModelIndex& index, int role) const
{
    const auto month = index.row();

    if (!hasIndex(month, index.column(), index.parent()))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return d->locale.standaloneMonthName(month);
        case MonthRole:
            return month;
        case HasArchiveRole:
            return d->hourHasArchive[(int) PeriodStorageType::currentCamera][month];
        case AnyCameraHasArchiveRole:
            return d->hourHasArchive[(int) PeriodStorageType::allCameras][month];
    }

    return QVariant();
}

QHash<int, QByteArray> MonthListModel::roleNames() const
{
    static QHash<int, QByteArray> roleNames;
    if (roleNames.isEmpty())
    {
        roleNames = QAbstractListModel::roleNames();
        roleNames.insert({
            {MonthRole, "month"},
            {HasArchiveRole, "hasArchive"},
            {AnyCameraHasArchiveRole, "anyCameraHasArchive"},
        });
    }
    return roleNames;
}

} // namespace nx::vms::client::core
