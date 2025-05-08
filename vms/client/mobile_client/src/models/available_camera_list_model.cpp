// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "available_camera_list_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <utils/common/counter_hash.h>
#include <watchers/available_cameras_watcher.h>

using namespace nx::vms::client::core;

using MobileContextAccessor = nx::vms::client::mobile::SystemContextAccessor;

class QnAvailableCameraListModelPrivate: public QObject
{
    QnAvailableCameraListModel* q_ptr;
    Q_DECLARE_PUBLIC(QnAvailableCameraListModel)

public:
    QList<QnResourcePtr> resources;
    QnLayoutResourcePtr layout;

public:
    QnAvailableCameraListModelPrivate(QnAvailableCameraListModel* parent);

    void setLayout(const QnLayoutResourcePtr& newLayout);
    void resetResources();

    void addCamera(const QnResourcePtr& resource, bool silent = false);
    void removeCamera(const QnResourcePtr& resource, bool silent = false);

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

    const auto& resource = d->resources[index.row()];

    switch (role)
    {
        case RawResourceRole:
            return QVariant::fromValue(withCppOwnership(resource));
        case ResourceNameRole:
            return resource->getName();
        case ResourceStatusRole:
            return (int) resource->getStatus();
        case UuidRole:
            return QVariant::fromValue(resource->getId());
        case IpAddressRole:
            return QUrl(resource->getUrl()).host();
    }
    return QVariant();
}

QHash<int, QByteArray> QnAvailableCameraListModel::roleNames() const
{
    return clientCoreRoleNames();
}

void QnAvailableCameraListModel::refreshResource(const QnResourcePtr& resource, int role)
{
    Q_D(QnAvailableCameraListModel);

    const auto row = d->resources.indexOf(resource);
    if (row == -1)
        return;

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

QModelIndex QnAvailableCameraListModel::indexByResourceId(const nx::Uuid& resourceId) const
{
    Q_D(const QnAvailableCameraListModel);

    const auto it = std::find_if(d->resources.cbegin(), d->resources.cend(),
        [resourceId](const auto resource)
        {
            return resource->getId() == resourceId;
        });

    return it == d->resources.cend()
        ? QModelIndex{}
        : index(std::distance(d->resources.cbegin(), it));
}

bool QnAvailableCameraListModel::filterAcceptsResource(const QnResourcePtr& resource) const
{
    if (!resource->hasFlags(Qn::live_cam))
        return false;

    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    return true;
}

QnAvailableCameraListModelPrivate::QnAvailableCameraListModelPrivate(QnAvailableCameraListModel* parent) :
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

    for (const auto& resource: resources)
        disconnect(resource.get(), nullptr, this, nullptr);
    resources.clear();

    if (layout)
    {
        for (const auto& item: layout->getItems())
        {
            if (const auto camera = nx::vms::client::core::getResourceByDescriptor(
                item.resource).dynamicCast<QnVirtualCameraResource>())
            {
                addCamera(camera, true);
            }
        }
    }
    else
    {
        const auto cameras = camerasWatcher->availableCameras();
        for (const auto& camera: cameras)
            addCamera(camera, true);
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

void QnAvailableCameraListModelPrivate::addCamera(const QnResourcePtr& resource, bool silent)
{
    if (resources.contains(resource))
        return;

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

    resources.append(resource);

    if (!silent)
        q->endInsertRows();

    if (auto systemContext = resource->systemContext()->as<nx::vms::client::mobile::SystemContext>();
        NX_ASSERT(systemContext))
    {
        if (systemContexts.insert(systemContext))
            emit q->systemContextAdded(systemContext);
    }
}

void QnAvailableCameraListModelPrivate::removeCamera(const QnResourcePtr& resource, bool silent)
{
    Q_Q(QnAvailableCameraListModel);

    const auto row = resources.indexOf(resource);
    if (row == -1)
        return;

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
    addCamera(resource);
}

void QnAvailableCameraListModelPrivate::at_watcher_cameraRemoved(const QnResourcePtr& resource)
{
    removeCamera(resource);
}

void QnAvailableCameraListModelPrivate::at_layout_itemAdded(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item)
{
    if (!NX_ASSERT(resource == layout && layout))
        return;

    const auto pool = MobileContextAccessor(layout).resourcePool();
    const auto camera = pool
        ? pool->getResourceById<QnVirtualCameraResource>(item.resource.id)
        : QnVirtualCameraResourcePtr{};

    if (camera)
        addCamera(camera);
}

void QnAvailableCameraListModelPrivate::at_layout_itemRemoved(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item)
{
    if (!NX_ASSERT(resource == layout && layout))
        return;

    const auto pool = MobileContextAccessor(layout).resourcePool();
    const auto camera = pool
        ? pool->getResourceById<QnVirtualCameraResource>(item.resource.id)
        : QnVirtualCameraResourcePtr{};

    if (camera)
        addCamera(camera);
}

void QnAvailableCameraListModelPrivate::handleResourceChanged(const QnResourcePtr& resource)
{
    Q_Q(QnAvailableCameraListModel);
    q->refreshResource(resource);
}
