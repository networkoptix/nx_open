#include "server_updates_model.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>


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

QnServerUpdatesModel::Item::Item(const QnMediaServerResourcePtr &server) :
    m_server(server),
    m_stage(QnPeerUpdateStage::Init)
{
}

QnServerUpdatesModel::QnServerUpdatesModel(QnMediaServerUpdateTool* tool, QObject *parent) :
    QAbstractTableModel(parent),
    QnWorkbenchContextAware(parent),
    m_updateTool(tool),
    m_checkResult(QnCheckForUpdateResult::BadUpdateFile)
{
    connect(m_updateTool,  &QnMediaServerUpdateTool::peerStageChanged,  this, [this](const QUuid &peerId, QnPeerUpdateStage stage) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;
        m_items[idx.row()]->m_stage = stage;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
    });

    connect(m_updateTool,  &QnMediaServerUpdateTool::peerStageProgressChanged,  this, [this](const QUuid &peerId, QnPeerUpdateStage stage, int progress) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;
        
        int displayStage = qMax(static_cast<int>(stage) - 1, 0);
        int value = (displayStage*100 + progress) / ( static_cast<int>(QnPeerUpdateStage::Count) - 1 );

        m_items[idx.row()]->m_stage = stage;
        m_items[idx.row()]->m_progress = value;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
    });

    connect(m_updateTool, &QnMediaServerUpdateTool::targetsChanged,  this,  &QnServerUpdatesModel::setTargets);

    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(), &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged,  this, &QnServerUpdatesModel::updateVersionColumn);

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
        if (m_latestVersion <= item->m_server->getVersion())
            return m_colors.latest;

        if (m_checkResult.systems.contains(item->m_server->getSystemInfo()))
            return m_colors.target;

        return m_colors.error;
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

    QHash<QnMediaServerResourcePtr, Item*> existingItems;
    foreach (Item* item, m_items)
        existingItems[item->server()] = item;

    m_items.clear();

    if (m_targets.isEmpty()) {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::server)) {
            QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
            if (existingItems.contains(server))
                m_items.append(existingItems.take(server));
            else
                m_items.append(new Item(server));
        }
    } else {
        foreach (const QUuid &id, m_targets) {
            QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
            if (!server)
                continue;
            if (existingItems.contains(server))
                m_items.append(existingItems.take(server));
            else
                m_items.append(new Item(server));
        }
    }
    qDeleteAll(existingItems.values());

    endResetModel();
}

void QnServerUpdatesModel::updateVersionColumn() {
    if (m_items.isEmpty())
        return;
    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
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
    updateVersionColumn();
}


QnCheckForUpdateResult QnServerUpdatesModel::checkResult() const {
    return m_checkResult;
}

void QnServerUpdatesModel::setCheckResult(const QnCheckForUpdateResult &result) {
    m_checkResult = result;
    updateVersionColumn();
}

QnServerUpdatesColors QnServerUpdatesModel::colors() const {
    return m_colors;
}

void QnServerUpdatesModel::setColors(const QnServerUpdatesColors &colors) {
    m_colors = colors;

    if (m_items.isEmpty())
        return;
    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}
