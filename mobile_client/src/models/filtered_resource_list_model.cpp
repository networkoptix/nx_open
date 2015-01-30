#include "filtered_resource_list_model.h"

#include "core/resource_management/resource_pool.h"
#include "mobile_client/mobile_client_roles.h"

QnFilteredResourceListModel::QnFilteredResourceListModel(QObject *parent) :
    QAbstractListModel(parent)
{
    resetResourcesInternal();
}

QnFilteredResourceListModel::~QnFilteredResourceListModel() {

}

void QnFilteredResourceListModel::resetResources() {
    beginResetModel();
    resetResourcesInternal();
    endResetModel();
}

int QnFilteredResourceListModel::rowCount(const QModelIndex &parent) const {
    return m_resources.size();
}

QVariant QnFilteredResourceListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column()))
        return QVariant();

    QnResourcePtr resource = m_resources[index.row()];

    switch (role) {
    case Qn::ResourceNameRole:
        return resource->getName();
    case Qn::UuidRole:
        return resource->getId().toString();
    case Qn::IpAddressRole:
        return QUrl(resource->getUrl()).host();
    }
}

QHash<int, QByteArray> QnFilteredResourceListModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[Qn::ResourceNameRole] = Qn::roleName(Qn::ResourceNameRole);
    roleNames[Qn::UuidRole] = Qn::roleName(Qn::UuidRole);
    roleNames[Qn::IpAddressRole] = Qn::roleName(Qn::IpAddressRole);
    return roleNames;
}

void QnFilteredResourceListModel::refreshResource(const QnResourcePtr &resource, int role) {
    int row = m_resources.indexOf(resource);
    if (row == -1)
        return;

    QVector<int> roles;
    if (role != -1)
        roles.append(role);
    QModelIndex index = this->index(row);
    emit dataChanged(index, index, roles);
}

void QnFilteredResourceListModel::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if (!filterAcceptsResource(resource))
        return;

    beginInsertRows(QModelIndex(), m_resources.size(), m_resources.size());
    m_resources.append(resource);
    endInsertRows();
}

void QnFilteredResourceListModel::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    int row = m_resources.indexOf(resource);
    if (row == -1)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_resources.removeAt(row);
    endRemoveRows();
}

void QnFilteredResourceListModel::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    int row = m_resources.indexOf(resource);
    bool accept = filterAcceptsResource(resource);

    if (row == -1) {
        if (accept)
            at_resourcePool_resourceAdded(resource);
    } else {
        if (!accept) {
            at_resourcePool_resourceRemoved(resource);
        } else {
            QModelIndex index = this->index(row);
            emit dataChanged(index, index);
        }
    }
}

void QnFilteredResourceListModel::resetResourcesInternal() {
    disconnect(qnResPool, 0, this, 0);

    m_resources.clear();
    foreach (const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(qnResPool,      &QnResourcePool::resourceAdded,     this,   &QnFilteredResourceListModel::at_resourcePool_resourceAdded);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,   this,   &QnFilteredResourceListModel::at_resourcePool_resourceRemoved);
    connect(qnResPool,      &QnResourcePool::resourceChanged,   this,   &QnFilteredResourceListModel::at_resourcePool_resourceChanged);
}

