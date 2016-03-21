#include "available_camera_list_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <mobile_client/mobile_client_roles.h>
#include <common/common_module.h>
#include <watchers/available_cameras_watcher.h>

QnAvailableCameraListModel::QnAvailableCameraListModel(QObject *parent) :
    base_type(parent)
{
}

QnAvailableCameraListModel::~QnAvailableCameraListModel() {

}

void QnAvailableCameraListModel::resetResources() {
    beginResetModel();
    resetResourcesInternal();
    endResetModel();
}

int QnAvailableCameraListModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return m_resources.size();
}

QVariant QnAvailableCameraListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column()))
        return QVariant();

    QnResourcePtr resource = m_resources[index.row()];

    switch (role) {
    case Qn::ResourceNameRole:
        return resource->getName();
    case Qn::ResourceStatusRole:
        return resource->getStatus();
    case Qn::UuidRole:
        return resource->getId().toString();
    case Qn::IpAddressRole:
        return QUrl(resource->getUrl()).host();
    }
    return QVariant();
}

QHash<int, QByteArray> QnAvailableCameraListModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[Qn::ResourceNameRole] = Qn::roleName(Qn::ResourceNameRole);
    roleNames[Qn::UuidRole] = Qn::roleName(Qn::UuidRole);
    roleNames[Qn::IpAddressRole] = Qn::roleName(Qn::IpAddressRole);
    roleNames[Qn::ResourceStatusRole] = Qn::roleName(Qn::ResourceStatusRole);
    return roleNames;
}

void QnAvailableCameraListModel::refreshResource(const QnResourcePtr &resource, int role) {
    int row = m_resources.indexOf(resource);
    if (row == -1)
        return;

    QVector<int> roles;
    if (role != -1)
        roles.append(role);
    QModelIndex index = this->index(row);
    emit dataChanged(index, index, roles);
}

bool QnAvailableCameraListModel::filterAcceptsResource(const QnResourcePtr &resource) const {
    if (!resource->hasFlags(Qn::live_cam))
        return false;

    if (resource->hasFlags(Qn::desktop_camera) || resource->hasFlags(Qn::io_module))
        return false;

    return true;
}

void QnAvailableCameraListModel::at_watcher_cameraAdded(const QnResourcePtr &resource) {
    if (!filterAcceptsResource(resource))
        return;

    connect(resource,   &QnResource::nameChanged,       this,   &QnAvailableCameraListModel::at_resourcePool_resourceChanged);
    connect(resource,   &QnResource::statusChanged,     this,   &QnAvailableCameraListModel::at_resourcePool_resourceChanged);

    beginInsertRows(QModelIndex(), m_resources.size(), m_resources.size());
    m_resources.append(resource);
    endInsertRows();
}

void QnAvailableCameraListModel::at_watcher_cameraRemoved(const QnResourcePtr &resource) {
    int row = m_resources.indexOf(resource);
    if (row == -1)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_resources.removeAt(row);
    endRemoveRows();

    disconnect(resource, nullptr, this, nullptr);
}

void QnAvailableCameraListModel::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnAvailableCamerasWatcher *watcher = qnCommon->instance<QnAvailableCamerasWatcher>();

    int row = m_resources.indexOf(resource);
    bool accept = filterAcceptsResource(resource) && watcher->isCameraAvailable(resource->getId());

    if (row == -1) {
        if (accept)
            at_watcher_cameraAdded(resource);
    } else {
        if (!accept) {
            at_watcher_cameraRemoved(resource);
        } else {
            QModelIndex index = this->index(row);
            emit dataChanged(index, index);
        }
    }
}

void QnAvailableCameraListModel::resetResourcesInternal() {
    QnAvailableCamerasWatcher *watcher = qnCommon->instance<QnAvailableCamerasWatcher>();

    disconnect(watcher, nullptr, this, nullptr);
    disconnect(qnResPool, nullptr, this, nullptr);

    m_resources.clear();
    for (const QnVirtualCameraResourcePtr &camera: watcher->availableCameras())
        at_watcher_cameraAdded(camera);

    connect(watcher,    &QnAvailableCamerasWatcher::cameraAdded,    this,   &QnAvailableCameraListModel::at_watcher_cameraAdded);
    connect(watcher,    &QnAvailableCamerasWatcher::cameraRemoved,  this,   &QnAvailableCameraListModel::at_watcher_cameraRemoved);
    connect(qnResPool,  &QnResourcePool::resourceChanged,           this,   &QnAvailableCameraListModel::at_resourcePool_resourceChanged);
}
