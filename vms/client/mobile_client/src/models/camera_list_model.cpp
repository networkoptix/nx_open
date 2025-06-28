// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_list_model.h"

#include <QtCore/QPointer>
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
#include <nx/utils/scoped_connections.h>
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

using namespace nx::vms::client;

struct QnCameraListModel::Private: public QObject
{
    Private(QnCameraListModel* q);

    void handleThumbnailUpdated(const QnVirtualCameraResourcePtr& camera);
    QModelIndex indexByResourceId(const nx::Uuid& resourceId) const;

    QnCameraListModel* const q;
    std::unique_ptr<QnAvailableCameraListModel> model;
    std::map<mobile::SystemContext*, nx::utils::ScopedConnection> systemContextConnections;
    QSet<nx::Uuid> filterIds;
    QSet<nx::Uuid> selectedIds;
};

QnCameraListModel::Private::Private(QnCameraListModel* q):
    q(q),
    model(std::make_unique<QnAvailableCameraListModel>())
{
    connect(model.get(), &QnAvailableCameraListModel::systemContextAdded, this,
        [this](mobile::SystemContext* systemContext)
        {
            systemContextConnections[systemContext].reset(connect(
                systemContext->cameraThumbnailCache(),
                &QnCameraThumbnailCache::thumbnailUpdated,
                this,
                [this, systemContext = QPointer<core::SystemContext>(systemContext)](
                    const nx::Uuid& resourceId)
                {
                    if (!systemContext)
                        return;

                    const auto camera = systemContext->resourcePool()->getResourceById<
                        QnVirtualCameraResource>(resourceId);

                    if (camera)
                        handleThumbnailUpdated(camera);
                }));
        });

    connect(model.get(), &QnAvailableCameraListModel::systemContextRemoved, this,
        [this](mobile::SystemContext* systemContext)
        {
            systemContextConnections.erase(systemContext);
        });
}

QnCameraListModel::QnCameraListModel(QObject* parent):
    base_type(parent),
    mobile::WindowContextAware(core::WindowContext::fromQmlContext(this)),
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
    roleNames.insert(core::clientCoreRoleNames());
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

    if (role != core::ThumbnailRole)
        return base_type::data(index, role);


    const mobile::SystemContextAccessor accessor(index);
    const auto cache = accessor.cameraThumbnailCache();
    if (!NX_ASSERT(cache))
        return {};

    const auto thumbnailId = cache->thumbnailId(accessor.rawResource()->getId());
    return thumbnailId.isEmpty()
        ? QUrl{}
        : QUrl("image://thumbnail/" + cache->cacheId() + "/" + thumbnailId);
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
    // TODO: #vkutin #ynikitenkov This doesn't support cross-system layouts and is unsafe.
    // However, currently it's not used in "layout mode".

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

    const auto found = match(index(0, 0), core::RawResourceRole, QVariant::fromValue(rawResource),
        /*hits*/ 1, Qt::MatchExactly);

    return found.empty() ? -1 : found.front().row();
}

nx::Uuid QnCameraListModel::resourceIdByRow(int row) const
{
    if (!hasIndex(row, 0))
        return {};

    return data(index(row, 0), core::UuidRole).value<nx::Uuid>();
}

QnResource* QnCameraListModel::nextResource(QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResource(resource), rowCount(), Step::nextRow);
    return data(index(row, 0), core::RawResourceRole).value<QnResource*>();
}

QnResource* QnCameraListModel::previousResource(QnResource* resource) const
{
    if (rowCount() == 0 || !resource)
        return nullptr;

    const int row = getRow(rowByResource(resource), rowCount(), Step::prevRow);
    return data(index(row, 0), core::RawResourceRole).value<QnResource*>();
}

void QnCameraListModel::refreshThumbnail(int row)
{
    if (!hasIndex(row, 0))
        return;

    if (const mobile::SystemContextAccessor accessor(index(row, 0));
        NX_ASSERT(accessor.systemContext()))
    {
        accessor.cameraThumbnailCache()->refreshThumbnail(accessor.rawResource()->getId());
    }
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
    const auto leftName = left.data(core::ResourceNameRole).toString();
    const auto rightName = right.data(core::ResourceNameRole).toString();

    int res = nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive);
    if (res != 0)
        return res < 0;

    const auto leftAddress = left.data(core::IpAddressRole).toString();
    const auto rightAddress = right.data(core::IpAddressRole).toString();

    res = nx::utils::naturalStringCompare(leftAddress, rightAddress);
    if (res != 0)
        return res < 0;

    const auto leftId = left.data(core::UuidRole).toString();
    const auto rightId = right.data(core::UuidRole).toString();

    return leftId < rightId;
}

bool QnCameraListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto name = index.data(core::ResourceNameRole).toString();
    if (!filterRegularExpression().match(name).hasMatch())
        return false;

    if (d->filterIds.isEmpty())
        return true;

    const auto id = index.data(core::UuidRole).value<nx::Uuid>();
    return d->filterIds.contains(id);
}

void QnCameraListModel::Private::handleThumbnailUpdated(const QnVirtualCameraResourcePtr& camera)
{
    model->refreshResource(camera, core::ThumbnailRole);
}

QModelIndex QnCameraListModel::Private::indexByResourceId(const nx::Uuid& resourceId) const
{
    const auto found = q->match(q->index(0, 0), core::UuidRole, QVariant::fromValue(resourceId),
        /*hits*/ 1, Qt::MatchExactly);

    return found.empty() ? QModelIndex{} : found.front();
}
