#include "server_updates_model.h"

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>

QnServerUpdatesModel::QnServerUpdatesModel(QObject *parent) :
    QAbstractTableModel(parent)
{
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
            QnSoftwareVersion updateVersion = m_updates.value(server->getSystemInfo());
            if (updateVersion.isNull())
                return tr("Not found");
            else
                return updateVersion.toString(QnSoftwareVersion::FullFormat);
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

void QnServerUpdatesModel::setServers(const QList<QnMediaServerResourcePtr> &servers) {
    beginResetModel();
    m_servers = servers;
    endResetModel();
}

void QnServerUpdatesModel::setUpdates(const QHash<QnSystemInformation, QnSoftwareVersion> &updates) {
    beginResetModel();
    m_updates = updates;
    endResetModel();

}
