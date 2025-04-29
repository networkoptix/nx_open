// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_list_model.h"

#include <QtCore/QUrlQuery>

#include <camera/camera_thumbnail_cache.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <mobile_client/mobile_client_settings.h>
#include <models/available_camera_list_model.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/utils/string.h>
#include <nx/utils/qt_helpers.h>

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
using MobileContextAccessor = nx::vms::client::mobile::SystemContextAccessor;

struct QnCameraListModel::Private: public QObject
{
    Private(QnCameraListModel* q);

    void handleThumbnailUpdated(const nx::Uuid& resourceId, const QString& thumbnailId);
    QModelIndex indexByResourceId(const nx::Uuid& resourceId) const;
    void updateSystemContext();

    QnCameraListModel* const q;
    std::unique_ptr<QnAvailableCameraListModel> model;
    QSet<nx::Uuid> filterIds;
    QSet<nx::Uuid> selectedIds;
    QPointer<nx::vms::client::mobile::SystemContext> currentContext;
};

QnCameraListModel::Private::Private(QnCameraListModel* q):
    q(q),
    model(std::make_unique<QnAvailableCameraListModel>())
{
}

QnCameraListModel::QnCameraListModel(QObject* parent):
    base_type(parent),
    nx::vms::client::mobile::WindowContextAware(WindowContext::fromQmlContext(this)),
    d(new Private(this))
{
    setSourceModel(d->model.get());
    setDynamicSortFilter(true);
    sort(0);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

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


    const MobileContextAccessor accessor(index);
    const auto cache = accessor.cameraThumbnailCache();
    if (!NX_ASSERT(cache))
        return {};

    const auto thumbnailId = cache->thumbnailId(accessor.rawResource()->getId());
    return thumbnailId.isEmpty()
        ? QUrl{}
        : QUrl("image://thumbnail/" + thumbnailId);
}

QnLayoutResource* QnCameraListModel::rawLayout() const
{
    return d->model->layout().get();
}

void QnCameraListModel::setRawLayout(QnLayoutResource* value)
{
    const auto layout = value
        ? value->toSharedPointer().dynamicCast<QnLayoutResource>()
        : QnLayoutResourcePtr{};

    const auto currentLayout = d->model->layout();
    if (currentLayout == layout)
        return;

    d->model->setLayout(layout);

    emit layoutChanged();
}

QVariant QnCameraListModel::filterIds() const
{
    return QVariant::fromValue(d->filterIds.values());
}

void QnCameraListModel::setFilterIds(const QVariant &ids)
{
    d->filterIds = nx::utils::toQSet(ids.value<QList<nx::Uuid>>());
    invalidateFilter();
}

int QnCameraListModel::count() const
{
    return rowCount();
}

UuidList QnCameraListModel::selectedIds() const
{
    return UuidList(d->selectedIds.begin(), d->selectedIds.end());
}

void QnCameraListModel::setSelectedIds(const UuidList& value)
{
    const UuidSet newValues(value.begin(), value.end());
    if (newValues == d->selectedIds)
        return;

    const auto changed =
        (newValues - d->selectedIds) //< Added items.
        + (d->selectedIds - newValues); //< Removed items

    d->selectedIds = newValues;

    for (const auto& id: changed)
    {
        const auto resourceIndex = d->indexByResourceId(id);
        if (!resourceIndex.isValid())
            continue;

        if (const int row = resourceIndex.row(); row >= 0)
            emit dataChanged(index(row, 0), index(row, 0), {Qt::CheckStateRole});
    }
    emit selectedIdsChagned();
}

void QnCameraListModel::setSelected(int row, bool selected)
{
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

int QnCameraListModel::rowByResource(QnResource* rawResource) const
{
    if (!rawResource)
        return -1;

    const auto resource = rawResource
        ? rawResource->toSharedPointer()
        : QnResourcePtr{};

    const auto index = d->model->indexByResourceId(resource->getId());
    return index.isValid()
        ? mapFromSource(index).row()
        : -1;
}

nx::Uuid QnCameraListModel::resourceIdByRow(int row) const
{
    if (!hasIndex(row, 0))
        return {};

    return data(index(row, 0), UuidRole).value<nx::Uuid>();
}

QnResource* QnCameraListModel::nextResource(QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResource(resource), rowCount(), Step::nextRow);
    return data(index(row, 0), RawResourceRole).value<QnResource*>();
}

QnResource* QnCameraListModel::previousResource(QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResource(resource), rowCount(), Step::prevRow);
    return data(index(row, 0), RawResourceRole).value<QnResource*>();
}

void QnCameraListModel::refreshThumbnail(int row)
{
    if (!hasIndex(row, 0))
        return;

    if (const MobileContextAccessor accessor(index(row, 0)); NX_ASSERT(accessor.systemContext()))
        accessor.cameraThumbnailCache()->refreshThumbnail(accessor.rawResource()->getId());
}

void QnCameraListModel::refreshThumbnails(int from, int to)
{
    int rowCount = this->rowCount();

    if (from == -1)
        from = 0;

    if (to == -1)
        to = rowCount - 1;

    if (from >= rowCount || to >= rowCount || from > to)
        return;

    QList<nx::Uuid> ids;
    for (int row = from; row <= to; row++)
        refreshThumbnail(row);
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

    if (d->filterIds.isEmpty())
        return true;

    const auto id = index.data(UuidRole).value<nx::Uuid>();
    return d->filterIds.contains(id);
}

void QnCameraListModel::Private::handleThumbnailUpdated(
    const nx::Uuid& resourceId, const QString& /*thumbnailId*/)
{
    const auto pool = SystemContextAccessor(indexByResourceId(resourceId)).resourcePool();
    if (NX_ASSERT(pool))
        model->refreshResource(pool->getResourceById(resourceId), ThumbnailRole);
}

QModelIndex QnCameraListModel::Private::indexByResourceId(const nx::Uuid& resourceId) const
{
    const auto sourceIndex = model->indexByResourceId(resourceId);
    if (!sourceIndex.isValid())
        return {};

    return q->mapFromSource(sourceIndex);
}

void QnCameraListModel::Private::updateSystemContext()
{
    QPointer<nx::vms::client::mobile::SystemContext> targetContext(
        MobileContextAccessor(model->layout()).mobileSystemContext());

    if (!targetContext)
        targetContext = q->windowContext()->mainSystemContext();

    if (targetContext == currentContext)
        return;

    if (currentContext)
        currentContext->cameraThumbnailCache()->disconnect(this);

    currentContext = targetContext;

    if (targetContext)
    {
        connect(currentContext->cameraThumbnailCache(), &QnCameraThumbnailCache::thumbnailUpdated,
            this, &QnCameraListModel::Private::handleThumbnailUpdated);
    }
}

void QnCameraListModel::onContextReady()
{
    connect(windowContext(), &nx::vms::client::mobile::WindowContext::mainSystemContextChanged,
        d.get(), &Private::updateSystemContext);
    d->updateSystemContext();
}
