// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_list_model.h"

#include <QtCore/QUrlQuery>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/utils/qt_helpers.h>
#include <models/available_camera_list_model.h>
#include <camera/camera_thumbnail_cache.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <mobile_client/mobile_client_settings.h>

namespace {

enum class Step
{
    nextRow,
    prevRow
};

int getRow(int currentRow, int rowCount, Step step)
{
    if (currentRow == -1)
        return currentRow;

    const int result = currentRow + (step == Step::nextRow ? 1 : -1);
    if (result < 0)
        return rowCount - 1;

    if (result >= rowCount)
        return 0;

    return result;
}
} // namespace

using namespace nx::vms::client::core;

class QnCameraListModelPrivate: public QObject
{
public:
    QnCameraListModelPrivate();

    void at_thumbnailUpdated(const nx::Uuid& resourceId, const QString& thumbnailId);

public:
    QnAvailableCameraListModel* model;
    QSet<nx::Uuid> filterIds;
    QSet<nx::Uuid> selectedIds;
};

QnCameraListModel::QnCameraListModel(QObject* parent) :
    base_type(parent),
    d_ptr(new QnCameraListModelPrivate())
{
    Q_D(QnCameraListModel);

    setSourceModel(d->model);
    setDynamicSortFilter(true);
    sort(0);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto cache = QnCameraThumbnailCache::instance();
    NX_ASSERT(cache);
    if (cache)
    {
        connect(cache, &QnCameraThumbnailCache::thumbnailUpdated,
                d, &QnCameraListModelPrivate::at_thumbnailUpdated);
    }

    connect(this, &QnCameraListModel::rowsInserted, this, &QnCameraListModel::countChanged);
    connect(this, &QnCameraListModel::rowsRemoved, this, &QnCameraListModel::countChanged);
    connect(this, &QnCameraListModel::modelReset, this, &QnCameraListModel::countChanged);
}

QnCameraListModel::~QnCameraListModel()
{
}

QHash<int, QByteArray> QnCameraListModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames.insert(clientCoreRoleNames());
    roleNames.insert(Qt::CheckStateRole, "checkState");
    return roleNames;
}

QVariant QnCameraListModel::data(const QModelIndex& index, int role) const
{
    Q_D(const QnCameraListModel);

    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (role == Qt::CheckStateRole)
    {
        const auto resourceId = resourceIdByRow(index.row());

        return d->selectedIds.contains(resourceId)
            ? Qt::Checked
            : Qt::Unchecked;
    }

    if (role != ThumbnailRole)
        return base_type::data(index, role);

    const auto cache = QnCameraThumbnailCache::instance();
    NX_ASSERT(cache);
    if (!cache)
        return QUrl();

    const auto uuid = base_type::data(index, UuidRole).value<nx::Uuid>();
    const auto id = nx::Uuid::fromStringSafe(uuid.toString());
    if (id.isNull())
        return QUrl();

    const auto thumbnailId = cache->thumbnailId(id);
    if (thumbnailId.isEmpty())
        return QUrl();

    return QUrl(QStringLiteral("image://thumbnail/") + thumbnailId);
}

QString QnCameraListModel::layoutId() const
{
    Q_D(const QnCameraListModel);
    const auto layout = d->model->layout();
    return layout ? layout->getId().toString() : QString();
}

void QnCameraListModel::setLayoutId(const QString& layoutId)
{
    Q_D(QnCameraListModel);

    QnLayoutResourcePtr layout;

    const auto id = nx::Uuid::fromStringSafe(layoutId);
    if (!id.isNull())
        layout = resourcePool()->getResourceById<QnLayoutResource>(id);

    if (d->model->layout() == layout)
        return;

    d->model->setLayout(layout);
    emit layoutIdChanged();
}

QVariant QnCameraListModel::filterIds() const
{
    Q_D(const QnCameraListModel);
    return QVariant::fromValue(d->filterIds.values());
}

void QnCameraListModel::setFilterIds(const QVariant &ids)
{
    Q_D(QnCameraListModel);
    d->filterIds = nx::utils::toQSet(ids.value<QList<nx::Uuid>>());
    invalidateFilter();
}

int QnCameraListModel::count() const
{
    return rowCount();
}

UuidList QnCameraListModel::selectedIds() const
{
    Q_D(const QnCameraListModel);
    return UuidList(d->selectedIds.begin(), d->selectedIds.end());
}

void QnCameraListModel::setSelectedIds(const UuidList& value)
{
    Q_D(QnCameraListModel);
    const UuidSet newValues(value.begin(), value.end());
    if (newValues == d->selectedIds)
        return;

    const auto changed =
        (newValues - d->selectedIds) //< Added items.
        + (d->selectedIds - newValues); //< Removed items

    d->selectedIds = newValues;

    for (const auto& id: changed)
    {
        const int row = rowByResourceId(id);
        if (row >= 0)
            emit dataChanged(index(row, 0), index(row, 0), {Qt::CheckStateRole});
    }
    emit selectedIdsChagned();
}

void QnCameraListModel::setSelected(int row, bool selected)
{
    Q_D(QnCameraListModel);
    const auto id = resourceIdByRow(row);
    if (id.isNull())
        return;

    if (d->selectedIds.contains(id) == selected)
        return; //< Nothing changed.

    if (selected)
        d->selectedIds.insert(id);
    else
        d->selectedIds.remove(id);

    emit dataChanged(index(row, 0), index(row, 0), {Qt::CheckStateRole});
    emit selectedIdsChagned();
}

int QnCameraListModel::rowByResourceId(const nx::Uuid& resourceId) const
{
    Q_D(const QnCameraListModel);

    const auto id = resourceId;
    if (id.isNull())
        return -1;

    auto index = d->model->indexByResourceId(id);
    if (!index.isValid())
        return -1;

    index = mapFromSource(index);

    return index.row();
}

nx::Uuid QnCameraListModel::resourceIdByRow(int row) const
{
    if (!hasIndex(row, 0))
        return nx::Uuid();

    return data(index(row, 0), UuidRole).value<nx::Uuid>();
}

QnResource* QnCameraListModel::nextResource(const QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResourceId(resource->getId()), rowCount(), Step::nextRow);
    return data(index(row, 0), RawResourceRole).value<QnResource*>();
}

QnResource* QnCameraListModel::previousResource(const QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResourceId(resource->getId()), rowCount(), Step::prevRow);
    return data(index(row, 0), RawResourceRole).value<QnResource*>();
}

void QnCameraListModel::refreshThumbnail(int row)
{
    if (!QnCameraThumbnailCache::instance())
        return;

    if (!hasIndex(row, 0))
        return;

    const auto id = data(index(row, 0), UuidRole).value<nx::Uuid>();
    QnCameraThumbnailCache::instance()->refreshThumbnail(id);
}

void QnCameraListModel::refreshThumbnails(int from, int to)
{
    if (!QnCameraThumbnailCache::instance())
        return;

    int rowCount = this->rowCount();

    if (from == -1)
        from = 0;

    if (to == -1)
        to = rowCount - 1;

    if (from >= rowCount || to >= rowCount || from > to)
        return;

    QList<nx::Uuid> ids;
    for (int i = from; i <= to; i++)
        ids.append(data(index(i, 0), UuidRole).value<nx::Uuid>());

    QnCameraThumbnailCache::instance()->refreshThumbnails(ids);
}

bool QnCameraListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftName = left.data(ResourceNameRole).toString();
    const auto rightName = right.data(ResourceNameRole).toString();

    int res = nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (res != 0)
        return res < 0;

    const auto leftAddress = left.data(IpAddressRole).toString();
    const auto rightAddress = right.data(IpAddressRole).toString();

    res = nx::utils::naturalStringCompare(leftAddress, rightAddress);
    if (res != 0)
        return res < 0;

    const auto leftId = left.data(UuidRole).toString();
    const auto rightId = right.data(UuidRole).toString();

    return leftId < rightId;
}

bool QnCameraListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto name = index.data(ResourceNameRole).toString();
    if (!filterRegularExpression().match(name).hasMatch())
        return false;

    Q_D(const QnCameraListModel);

    if (d->filterIds.isEmpty())
        return true;

    const auto id = index.data(UuidRole).value<nx::Uuid>();
    return d->filterIds.contains(id);
}

QnCameraListModelPrivate::QnCameraListModelPrivate() :
    model(new QnAvailableCameraListModel(this))
{
}

void QnCameraListModelPrivate::at_thumbnailUpdated(
    const nx::Uuid& resourceId, const QString& /*thumbnailId*/)
{
    model->refreshResource(model->resourcePool()->getResourceById(resourceId), ThumbnailRole);
}
