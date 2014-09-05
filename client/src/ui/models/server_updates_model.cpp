#include "server_updates_model.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>

QnServerUpdatesModel::QnServerUpdatesModel(QnMediaServerUpdateTool* tool, QObject *parent) :
    QAbstractTableModel(parent),
    m_updateTool(tool)
{
    connect(m_updateTool,  &QnMediaServerUpdateTool::peerChanged,          this,           [this](const QUuid &peerId) {
        setUpdateInformation(m_updateTool->updateInformation(peerId));
    });

    connect(m_updateTool, &QnMediaServerUpdateTool::stateChanged,    this,  [this]() {
        foreach (const QnMediaServerResourcePtr &server, m_updateTool->actualTargets())
            setUpdateInformation(m_updateTool->updateInformation(server->getId()));
        setLatestVersion(m_updateTool->targetVersion());
    });

    connect(m_updateTool, &QnMediaServerUpdateTool::targetsChanged,  this,  &QnServerUpdatesModel::setTargets);

    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnServerUpdatesModel::at_resourceChanged);

    setTargets(m_updateTool->actualTargetIds());
}

void QnServerUpdatesModel::setTargets(const QSet<QUuid> &targets) {
    m_targets = targets;
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
        case NameColumn:
            return tr("Server");
        case VersionColumn:
            return tr("Status");
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

    if (role == Qt::ForegroundRole && index.column() == VersionColumn) {
        if (item->m_updateInfo.state == QnPeerUpdateInformation::UpdateFound)
            return QColor(Qt::yellow);

        if (!m_latestVersion.isNull() &&
            item->m_updateInfo.state == QnPeerUpdateInformation::UpdateNotFound) {
                if (item->m_server->getVersion() == m_latestVersion)
                    return QColor(Qt::green);
                return QColor(Qt::red);
        }
        return QVariant();
    }

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

void QnServerUpdatesModel::setUpdatesInformation(const QHash<QUuid, QnPeerUpdateInformation> &updates) {
    foreach (Item *item, m_items)
        item->m_updateInfo = updates[item->server()->getId()];

    if (!m_items.isEmpty())
        emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}

void QnServerUpdatesModel::setUpdateInformation(const QnPeerUpdateInformation &update) {
    if (!update.server)
        return;

    m_updates[update.server->getId()] = update;

    for (int i = 0; i < m_items.size(); i++) {
        Item *item = m_items[i];
        if (item->server() == update.server) {
            item->m_updateInfo = update;
            emit dataChanged(index(i, VersionColumn), index(i, VersionColumn));
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

void QnServerUpdatesModel::at_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QModelIndex idx = index(server);
    if (!idx.isValid())
        return;

    emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
}

QnSoftwareVersion QnServerUpdatesModel::latestVersion() const {
    return m_latestVersion;
}

void QnServerUpdatesModel::setLatestVersion(const QnSoftwareVersion &version) {
    if (m_latestVersion == version)
        return;
    m_latestVersion = version;
    if (m_items.isEmpty())
        return;

    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}


QnMediaServerResourcePtr QnServerUpdatesModel::Item::server() const {
    return m_server;
}

QnPeerUpdateInformation QnServerUpdatesModel::Item::updateInformation() const {
    return m_updateInfo;
}

QVariant QnServerUpdatesModel::Item::data(int column, int role) const {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (column) {
        case NameColumn:
            return getResourceName(m_server);
        case VersionColumn:
            return m_server->getVersion().toString(QnSoftwareVersion::FullFormat);
        default:
            break;
        }
        break;
    case Qt::DecorationRole:
        if (column == NameColumn)
            return qnResIconCache->icon(m_server);
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
