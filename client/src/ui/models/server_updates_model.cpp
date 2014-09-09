#include "server_updates_model.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>


QnMediaServerResourcePtr QnServerUpdatesModel::Item::server() const {
    return m_server;
}

QnPeerUpdateStage QnServerUpdatesModel::Item::stage() const {
    return m_stage;
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

    case Qn::MediaServerResourceRole:
        return QVariant::fromValue<QnMediaServerResourcePtr>(m_server);

    case StageRole:
        return static_cast<int>(m_stage);
    case ProgressRole:
        return m_progress;
    default:
        break;
    }

    return QVariant();
}




QnServerUpdatesModel::QnServerUpdatesModel(QnMediaServerUpdateTool* tool, QObject *parent) :
    QAbstractTableModel(parent),
    m_updateTool(tool),
    m_checkResult(QnCheckForUpdateResult::NoNewerVersion)
{
    connect(m_updateTool,  &QnMediaServerUpdateTool::peerStageChanged,  this, [this](const QUuid &peerId, QnPeerUpdateStage stage) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;
        m_items[idx.row()]->m_stage = stage;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
    });

    connect(m_updateTool,  &QnMediaServerUpdateTool::peerProgressChanged,  this, [this](const QUuid &peerId, int progress) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;
        m_items[idx.row()]->m_progress = progress;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
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
        
        if (m_checkResult.result == QnCheckForUpdateResult::NoNewerVersion)
            return QColor(Qt::green);

        if (m_checkResult.result == QnCheckForUpdateResult::UpdateFound &&
            !m_checkResult.latestVersion.isNull()) {

            if (item->m_server->getVersion() == m_checkResult.latestVersion)
                return QColor(Qt::green);

            if (m_checkResult.systems.contains(item->m_server->getSystemInfo()))
                return QColor(Qt::yellow);

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

QModelIndex QnServerUpdatesModel::index(const QUuid &id) const {
    auto it = std::find_if(m_items.begin(), m_items.end(), [&id](Item *item){ return item->server()->getId() == id; });
    if (it == m_items.end())
        return QModelIndex();

    return base_type::index(it - m_items.begin(), 0);
}

void QnServerUpdatesModel::resetResourses() {
    beginResetModel();

    qDeleteAll(m_items);
    m_items.clear();

    if (m_targets.isEmpty()) {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::server)) {
            QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
            m_items.append(new Item(server));
        }
    } else {
        foreach (const QUuid &id, m_targets) {
            QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
            if (!server)
                continue;

            m_items.append(new Item(server));
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

QnCheckForUpdateResult QnServerUpdatesModel::checkResult() const {
    return m_checkResult;
}

void QnServerUpdatesModel::setCheckResult(const QnCheckForUpdateResult &result) {
    m_checkResult = result;
    if (m_items.isEmpty())
        return;

    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}
