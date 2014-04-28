#include "server_updates_model.h"

#include <core/resource_management/resource_pool.h>
#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>

QnServerUpdatesModel::QnServerUpdatesModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnServerUpdatesModel::at_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnServerUpdatesModel::at_resourceRemoved);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnServerUpdatesModel::at_resourceChanged);

    resetResourses();
}

int QnServerUpdatesModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int QnServerUpdatesModel::rowCount(const QModelIndex &parent) const {
    if(parent.isValid())
        return 0;

    return m_servers.size();
}

QVariant QnServerUpdatesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ResourceNameColumn:
            return tr("Server");
        case CurrentVersionColumn:
            return tr("Current Version");
        case UpdateColumn:
            return tr("Update Availability");
        default:
            break;
        }
    }
    return QVariant();
}

QVariant QnServerUpdatesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnMediaServerResourcePtr server = m_servers[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column()) {
        case ResourceNameColumn:
            return getResourceName(server);
        case CurrentVersionColumn:
            return server->getVersion().toString(QnSoftwareVersion::FullFormat);
        case UpdateColumn: {
            auto it = m_updates.find(server->getSystemInfo());
            if (it == m_updates.end())
                break;

            const UpdateInformation &updateInformation = it.value();
            switch (updateInformation.status) {
            case NotFound:
                return tr("Not found");
            case Found:
                return (updateInformation.version == server->getVersion()) ? tr("Not needed") : updateInformation.version.toString(QnSoftwareVersion::FullFormat);
            case Uploading:
                return QString::number(updateInformation.progress) + lit("%");
            case Installing:
                return tr("Installing...");
            case Installed:
                return tr("Installed");
            }
        }
        default:
            break;
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == ResourceNameColumn)
            return qnResIconCache->icon(server->flags(), server->getStatus());
        break;
    case Qt::BackgroundRole:
        break;
    }

    return QVariant();
}

QList<QnMediaServerResourcePtr> QnServerUpdatesModel::servers() const {
    return m_servers;
}

void QnServerUpdatesModel::setUpdatesInformation(const UpdateInformationHash &updates) {
    m_updates = updates;
    if (!m_servers.isEmpty())
        emit dataChanged(index(0, UpdateColumn), index(m_servers.size() - 1, UpdateColumn));
}

void QnServerUpdatesModel::resetResourses() {
    beginResetModel();

    m_servers.clear();
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server))
        m_servers.append(resource.staticCast<QnMediaServerResource>());

    endResetModel();
}

void QnServerUpdatesModel::at_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
    m_servers.append(server);
    endInsertRows();
}

void QnServerUpdatesModel::at_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    int index = m_servers.indexOf(server);
    if (index == -1)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_servers.removeAt(index);
    endRemoveRows();
}

void QnServerUpdatesModel::at_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    int i = m_servers.indexOf(server);
    if (i == -1)
        return;

    emit dataChanged(index(i, 0), index(i, ColumnCount));
}
