// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "available_camera_list_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <mobile_client/mobile_client_module.h>
#include <watchers/available_cameras_watcher.h>

using namespace nx::vms::client::core;

class QnAvailableCameraListModelPrivate:
    public QObject,
    public nx::vms::client::mobile::CurrentSystemContextAware
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
    void at_resourceChanged(const QnResourcePtr& resource);
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

    const auto resource = resourcePool()->getResourceById(resourceId);
    if (!resource)
        return QModelIndex();

    const auto row = d->resources.indexOf(resource);
    if (row < 0)
        return QModelIndex();

    return index(row);
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
    connect(resourcePool(), &QnResourcePool::resourceChanged,
            this, &QnAvailableCameraListModelPrivate::at_resourceChanged);

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
    const auto* camerasWatcher = qnMobileClientModule->availableCamerasWatcher();

    Q_Q(QnAvailableCameraListModel);

    q->beginResetModel();

    for (const auto& resource: resources)
        disconnect(resource.get(), nullptr, this, nullptr);
    resources.clear();

    if (layout)
    {
        for (const auto& item: layout->getItems())
        {
            const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(item.resource.id);
            if (camera)
                addCamera(camera, true);
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

    connect(resource.get(), &QnResource::nameChanged,
            this, &QnAvailableCameraListModelPrivate::at_resourceChanged);
    connect(resource.get(), &QnResource::statusChanged,
            this, &QnAvailableCameraListModelPrivate::at_resourceChanged);

    const auto row = resources.size();

    if (!silent)
        q->beginInsertRows(QModelIndex(), row, row);

    resources.append(resource);

    if (!silent)
        q->endInsertRows();
}

void QnAvailableCameraListModelPrivate::removeCamera(const QnResourcePtr& resource, bool silent)
{
    Q_Q(QnAvailableCameraListModel);

    const auto row = resources.indexOf(resource);
    if (row == -1)
        return;

    disconnect(resource.get(), nullptr, this, nullptr);

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
    NX_ASSERT(resource == layout);

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(item.resource.id);
    if (camera)
        addCamera(camera);
}

void QnAvailableCameraListModelPrivate::at_layout_itemRemoved(
        const QnLayoutResourcePtr& resource, const nx::vms::common::LayoutItemData& item)
{
    NX_ASSERT(resource == layout);

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(item.resource.id);
    if (camera)
        removeCamera(camera);
}

void QnAvailableCameraListModelPrivate::at_resourceChanged(const QnResourcePtr& resource)
{
    Q_Q(QnAvailableCameraListModel);
    q->refreshResource(resource);
}
