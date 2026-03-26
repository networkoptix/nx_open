// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "available_camera_list_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <utils/common/counter_hash.h>
#include <watchers/available_cameras_watcher.h>

using namespace nx::vms::client::core;

using MobileContextAccessor = nx::vms::client::mobile::SystemContextAccessor;

namespace {

struct ResourceEntry
{
    QnResourcePtr resource;
    std::set<nx::Uuid> layoutItemSourceIds;
};

} // namespace

class QnAvailableCameraListModelPrivate: public QObject
{
    QnAvailableCameraListModel* q_ptr;
    Q_DECLARE_PUBLIC(QnAvailableCameraListModel)

public:
    QList<ResourceEntry> resources;
    QnLayoutResourcePtr layout;

public:
    QnAvailableCameraListModelPrivate(QnAvailableCameraListModel* parent);

    void setLayout(const QnLayoutResourcePtr& newLayout);

private:
    void resetResources();

    void addCamera(const QnResourcePtr& resource, const nx::Uuid& sourceId, bool silent = false);
    void removeCamera(const QnResourcePtr& resource, const nx::Uuid& sourceId, bool silent = false);

    void at_watcher_cameraAdded(const QnResourcePtr& resource);
    void at_watcher_cameraRemoved(const QnResourcePtr& resource);
    void at_layout_itemAdded(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item);
    void at_layout_itemRemoved(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item);
    void handleResourceChanged(const QnResourcePtr& resource);

private:
    QnCounterHash<SystemContext*> systemContexts;
};

QnAvailableCameraListModel::QnAvailableCameraListModel(QObject* parent) :
    base_type(parent),
    d_ptr(new QnAvailableCameraListModelPrivate(this))
{
}

QnAvailableCameraListModel::~QnAvailableCameraListModel()
{
}

int QnAvailableCameraListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    Q_D(const QnAvailableCameraListModel);
    return d->resources.size();
}

QVariant QnAvailableCameraListModel::data(const QModelIndex& index, int role) const
{
    Q_D(const QnAvailableCameraListModel);

    if (index.row() > d->resources.size())
        return QVariant();

    const auto& resource = d->resources[index.row()].resource;

    switch (role)
    {
        case RawResourceRole:
            return QVariant::fromValue(withCppOwnership(resource));
        case ResourceNameRole:
            return resource->getName();
        case ResourceStatusRole:
            return static_cast<int>(resource->getStatus());
        case UuidRole:
            return QVariant::fromValue(resource->getId());
        case IpAddressRole:
            return QUrl(resource->getUrl()).host();
        case ResourceIconKeyRole:
            return static_cast<int>(appContext()->resourceIconCache()->key(resource));
        default:
            return QVariant{};
    }
}

QHash<int, QByteArray> QnAvailableCameraListModel::roleNames() const
{
    return clientCoreRoleNames();
}

void QnAvailableCameraListModel::refreshResource(const QnResourcePtr& resource, int role)
{
    Q_D(QnAvailableCameraListModel);

    const auto resourceIter = std::find_if(d->resources.begin(), d->resources.end(),
        [resource](const ResourceEntry& entry)
        {
            return entry.resource == resource;
        });

    if (resourceIter == d->resources.end())
        return;

    const int row = resourceIter - d->resources.begin();
    const auto idx = index(row);

    QVector<int> roles;
    if (role != -1)
        roles.append(role);

    emit dataChanged(idx, idx, roles);
}

QnLayoutResourcePtr QnAvailableCameraListModel::layout() const
{
    Q_D(const QnAvailableCameraListModel);
    return d->layout;
}

void QnAvailableCameraListModel::setLayout(const QnLayoutResourcePtr& layout)
{
    Q_D(QnAvailableCameraListModel);
    d->setLayout(layout);
}

bool QnAvailableCameraListModel::filterAcceptsResource(const QnResourcePtr& resource) const
{
    if (!resource->hasFlags(Qn::live_cam))
        return false;

    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    return true;
}

QnAvailableCameraListModelPrivate::QnAvailableCameraListModelPrivate(QnAvailableCameraListModel* parent):
    q_ptr(parent)
{
    resetResources();
}

void QnAvailableCameraListModelPrivate::setLayout(const QnLayoutResourcePtr& newLayout)
{
    if (layout == newLayout)
        return;

    if (layout)
        disconnect(layout.get(), nullptr, this, nullptr);

    layout = newLayout;

    resetResources();
}

void QnAvailableCameraListModelPrivate::resetResources()
{
    const auto systemContext = nx::vms::client::mobile::appContext()->currentSystemContext();
    if (!NX_ASSERT(systemContext))
        return;

    const auto camerasWatcher = systemContext->availableCamerasWatcher();

    Q_Q(QnAvailableCameraListModel);

    q->beginResetModel();

    for (const auto& resourceEntry: resources)
        disconnect(resourceEntry.resource.get(), nullptr, this, nullptr);
    resources.clear();

    if (layout)
    {
        for (const auto& item: layout->getItems())
        {
            if (const auto camera = nx::vms::client::core::getResourceByDescriptor(
                item.resource).dynamicCast<QnVirtualCameraResource>())
            {
                addCamera(camera, item.uuid, true);
            }
        }
    }
    else
    {
        const auto cameras = camerasWatcher->availableCameras();
        for (const auto& camera: cameras)
            addCamera(camera, nx::Uuid(), true);
    }

    q->endResetModel();

    if (layout)
    {
        disconnect(camerasWatcher, nullptr, this, nullptr);

        connect(layout.get(), &QnLayoutResource::itemAdded,
            this, &QnAvailableCameraListModelPrivate::at_layout_itemAdded);
        connect(layout.get(), &QnLayoutResource::itemRemoved,
            this, &QnAvailableCameraListModelPrivate::at_layout_itemRemoved);
        connect(layout.get(), &QnLayoutResource::itemChanged, this,
            [this](const QnLayoutResourcePtr& layout, const auto& item, const auto& oldItem)
            {
                if (item.resource == oldItem.resource)
                    return;
                at_layout_itemRemoved(layout, oldItem);
                at_layout_itemAdded(layout, item);
            });
    }
    else
    {
        connect(camerasWatcher, &QnAvailableCamerasWatcher::cameraAdded,
                this, &QnAvailableCameraListModelPrivate::at_watcher_cameraAdded);
        connect(camerasWatcher, &QnAvailableCamerasWatcher::cameraRemoved,
                this, &QnAvailableCameraListModelPrivate::at_watcher_cameraRemoved);
    }
}

void QnAvailableCameraListModelPrivate::addCamera(
    const QnResourcePtr& resource, const nx::Uuid& sourceId, bool silent)
{
    const auto resourceIter = std::find_if(resources.begin(), resources.end(),
        [resource](const ResourceEntry& entry)
        {
            return entry.resource == resource;
        });

    if (resourceIter != resources.end())
    {
        NX_ASSERT(!resourceIter->layoutItemSourceIds.contains(sourceId));
        if (!NX_ASSERT(!resourceIter->layoutItemSourceIds.empty()))
        {
            NX_WARNING(this,
                "Empty layoutItemSourceIds for resource: %1, sourceId: %2, layout: %3",
                resource->getId(),
                sourceId,
                layout ? layout->getId() : nx::Uuid());
        }

        resourceIter->layoutItemSourceIds.insert(sourceId);
        return;
    }

    Q_Q(QnAvailableCameraListModel);

    if (!q->filterAcceptsResource(resource))
        return;

    connect(resource.get(), &QnResource::resourceChanged,
        this, &QnAvailableCameraListModelPrivate::handleResourceChanged);
    connect(resource.get(), &QnResource::propertyChanged,
        this, &QnAvailableCameraListModelPrivate::handleResourceChanged);
    connect(resource.get(), &QnResource::nameChanged,
            this, &QnAvailableCameraListModelPrivate::handleResourceChanged);
    connect(resource.get(), &QnResource::statusChanged,
            this, &QnAvailableCameraListModelPrivate::handleResourceChanged);

    const auto row = resources.size();

    if (!silent)
        q->beginInsertRows(QModelIndex(), row, row);

    resources.append({resource, {sourceId}});

    if (!silent)
        q->endInsertRows();

    if (auto systemContext = resource->systemContext()->as<nx::vms::client::mobile::SystemContext>();
        NX_ASSERT(systemContext))
    {
        if (systemContexts.insert(systemContext))
            emit q->systemContextAdded(systemContext);
    }
}

void QnAvailableCameraListModelPrivate::removeCamera(
    const QnResourcePtr& resource, const nx::Uuid& sourceId, bool silent)
{
    Q_Q(QnAvailableCameraListModel);

    const auto resourceIter = std::find_if(resources.begin(), resources.end(),
        [resource](const ResourceEntry& entry)
        {
            return entry.resource == resource;
        });

    if (resourceIter == resources.end())
        return;

    if (!NX_ASSERT(resourceIter->layoutItemSourceIds.contains(sourceId)))
    {
        NX_WARNING(this,
            "layoutItemSourceId is not saved for resource: %1, item: %2, layout: %3",
            resource->getId(),
            sourceId,
            layout ? layout->getId() : nx::Uuid());
    }

    resourceIter->layoutItemSourceIds.erase(sourceId);
    if (!resourceIter->layoutItemSourceIds.empty())
        return;

    const int row = resourceIter - resources.begin();

    disconnect(resource.get(), nullptr, this, nullptr);

    if (auto systemContext = resource->systemContext()->as<nx::vms::client::mobile::SystemContext>();
        NX_ASSERT(systemContext))
    {
        if (systemContexts.remove(systemContext))
            emit q->systemContextAdded(systemContext);
    }

    if (!silent)
        q->beginRemoveRows(QModelIndex(), row, row);

    resources.removeAt(row);

    if (!silent)
        q->endRemoveRows();
}

void QnAvailableCameraListModelPrivate::at_watcher_cameraAdded(const QnResourcePtr& resource)
{
    addCamera(resource, nx::Uuid());
}

void QnAvailableCameraListModelPrivate::at_watcher_cameraRemoved(const QnResourcePtr& resource)
{
    removeCamera(resource, nx::Uuid());
}

void QnAvailableCameraListModelPrivate::at_layout_itemAdded(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item)
{
    if (!NX_ASSERT(resource == layout && layout))
        return;

    const auto camera = nx::vms::client::core::getResourceByDescriptor(item.resource);

    if (camera)
        addCamera(camera, item.uuid);
}

void QnAvailableCameraListModelPrivate::at_layout_itemRemoved(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item)
{
    if (!NX_ASSERT(resource == layout && layout))
        return;

    const auto camera = nx::vms::client::core::getResourceByDescriptor(item.resource);

    if (camera)
        removeCamera(camera, item.uuid);
}

void QnAvailableCameraListModelPrivate::handleResourceChanged(const QnResourcePtr& resource)
{
    Q_Q(QnAvailableCameraListModel);
    q->refreshResource(resource);
}
