#include "server_updates_model.h"

#include <common/common_module.h>
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

void QnServerUpdatesModel::setTargets(const QSet<QUuid> &targets) {
    m_targets = targets;
    resetResourses();
}

void QnServerUpdatesModel::setTargets(const QnMediaServerResourceList &targets) {
    m_targets.clear();
    foreach (const QnMediaServerResourcePtr &server, targets)
        m_targets.insert(server->getId());
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

    return m_items.size();
}

QVariant QnServerUpdatesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ResourceNameColumn:
            return tr("Server");
        case CurrentVersionColumn:
            return tr("Current Version");
        case UpdateColumn:
            return tr("Update Status");
        default:
            break;
        }
    }
    return QVariant();
}

QVariant QnServerUpdatesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    Item *item = reinterpret_cast<Item*>(index.internalPointer());

    return item->data(index.column(), role);
}

QModelIndex QnServerUpdatesModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column, m_items[row]);
}

QModelIndex QnServerUpdatesModel::index(const QnMediaServerResourcePtr &server) const {
    auto it = std::find_if(m_items.begin(), m_items.end(), [&server](Item *item){ return item->server() == server; });
    if (it == m_items.end())
        return QModelIndex();

    return base_type::index(it - m_items.begin(), 0);
}

void QnServerUpdatesModel::setUpdatesInformation(const QHash<QUuid, QnMediaServerUpdateTool::PeerUpdateInformation> &updates) {
    foreach (Item *item, m_items)
        item->m_updateInfo = updates[item->server()->getId()];

    if (!m_items.isEmpty())
        emit dataChanged(index(0, UpdateColumn), index(m_items.size() - 1, UpdateColumn));
}

void QnServerUpdatesModel::setUpdateInformation(const QnMediaServerUpdateTool::PeerUpdateInformation &update) {
    if (!update.server)
        return;

    m_updates[update.server->getId()] = update;

    for (int i = 0; i < m_items.size(); i++) {
        Item *item = m_items[i];
        if (item->server() == update.server) {
            item->m_updateInfo = update;
            emit dataChanged(index(i, UpdateColumn), index(i, UpdateColumn));
            break;
        }
    }
}

void QnServerUpdatesModel::resetResourses() {
    beginResetModel();

    qDeleteAll(m_items);
    m_items.clear();

    if (m_targets.isEmpty()) {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::server)) {
            QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
            m_items.append(new Item(server, m_updates[server->getId()]));
        }
    } else {
        foreach (const QUuid &id, m_targets) {
            QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
            if (!server)
                continue;

            m_items.append(new Item(server, m_updates[server->getId()]));
        }
    }

    endResetModel();
}

void QnServerUpdatesModel::at_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (m_targets.isEmpty()) {
        if (server->getSystemName() != qnCommon->localSystemName())
            return;
    } else {
        if (!m_targets.contains(resource->getId()))
            return;
    }

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(new Item(server, m_updates[server->getId()]));
    endInsertRows();
}

void QnServerUpdatesModel::at_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QModelIndex idx = index(server);
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());
    m_items.removeAt(idx.row());
    endRemoveRows();

    delete static_cast<Item*>(idx.internalPointer());
}

void QnServerUpdatesModel::at_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QModelIndex idx = index(server);
    if (!idx.isValid())
        return;

    emit dataChanged(idx, idx.sibling(idx.row(), LastColumn));
}


QnMediaServerResourcePtr QnServerUpdatesModel::Item::server() const {
    return m_server;
}

QnMediaServerUpdateTool::PeerUpdateInformation QnServerUpdatesModel::Item::updateInformation() const {
    return m_updateInfo;
}

QVariant QnServerUpdatesModel::Item::data(int column, int role) const {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (column) {
        case ResourceNameColumn:
            return getResourceName(m_server);
        case CurrentVersionColumn:
            return m_server->getVersion().toString(QnSoftwareVersion::FullFormat);
        case UpdateColumn: {
            switch (m_updateInfo.state) {
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateUnknown:
                return QString();
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateNotFound:
                return tr("Not found");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateFound:
                return (m_updateInfo.sourceVersion == m_updateInfo.updateInformation->version)
                        ? tr("Not needed") : m_updateInfo.updateInformation->version.toString(QnSoftwareVersion::FullFormat);
            case QnMediaServerUpdateTool::PeerUpdateInformation::PendingDownloading:
                return tr("Pending...");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateDownloading:
                return QString::number(m_updateInfo.progress) + lit("%");
            case QnMediaServerUpdateTool::PeerUpdateInformation::PendingUpload:
                return tr("Downloaded");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateUploading:
                return QString::number(m_updateInfo.progress) + lit("%");
            case QnMediaServerUpdateTool::PeerUpdateInformation::PendingInstallation:
                return tr("Uploaded");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateInstalling:
                return tr("Installing...");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateFinished:
                return tr("Finished");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateFailed:
                return tr("Failed");
            case QnMediaServerUpdateTool::PeerUpdateInformation::UpdateCanceled:
                return tr("Canceled");
            }
        }
        default:
            break;
        }
        break;
    case Qt::DecorationRole:
        if (column == ResourceNameColumn)
            return qnResIconCache->icon(m_server);
        break;
    case Qt::BackgroundRole:
        break;
    case StateRole:
        return m_updateInfo.state;
    case ProgressRole:
        return m_updateInfo.progress;
    default:
        break;
    }

    return QVariant();
}
