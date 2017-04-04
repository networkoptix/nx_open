#include "server_updates_model.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

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
            return QnResourceDisplayInfo(m_server).toString(qnSettings->extraInfoInTree());
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
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_updateTool(tool),
    m_checkResult(QnCheckForUpdateResult::BadUpdateFile)
{
    connect(m_updateTool,  &QnMediaServerUpdateTool::peerStageChanged,  this, [this](const QnUuid &peerId, QnPeerUpdateStage stage) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;
        m_items[idx.row()]->m_stage = stage;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
        updateLowestInstalledVersion();
    });

    connect(m_updateTool,  &QnMediaServerUpdateTool::peerStageProgressChanged,  this, [this](const QnUuid &peerId, QnPeerUpdateStage stage, int progress) {
        QModelIndex idx = index(peerId);
        if (!idx.isValid())
            return;

        int displayStage = qMax(static_cast<int>(stage) - 1, 0);
        int value = (displayStage*100 + progress) / ( static_cast<int>(QnPeerUpdateStage::Count) - 1 );

        m_items[idx.row()]->m_stage = stage;
        m_items[idx.row()]->m_progress = value;
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
    });

    connect(resourcePool(),  &QnResourcePool::resourceAdded,     this,   &QnServerUpdatesModel::at_resourceAdded);
    connect(resourcePool(),  &QnResourcePool::resourceRemoved,   this,   &QnServerUpdatesModel::at_resourceRemoved);
    connect(resourcePool(),  &QnResourcePool::resourceChanged,   this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(resourcePool(),  &QnResourcePool::statusChanged,     this,   &QnServerUpdatesModel::at_resourceChanged);
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(), &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged,  this, &QnServerUpdatesModel::updateVersionColumn);

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

QModelIndex QnServerUpdatesModel::index(const QnUuid &id) const {
    auto it = std::find_if(m_items.begin(), m_items.end(), [&id](Item *item){ return item->server()->getId() == id; });
    if (it == m_items.end())
        return QModelIndex();

    return base_type::index(it - m_items.begin(), 0);
}

void QnServerUpdatesModel::resetResourses() {
    beginResetModel();

    qDeleteAll(m_items);
    m_items.clear();

    const auto allServers = resourcePool()->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr &server: allServers)
        m_items.append(new Item(server));

    for (const QnResourcePtr &resource: resourcePool()->getAllIncompatibleResources())
    {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server || !helpers::serverBelongsToCurrentSystem(server))
            continue;

        // Adds newly added to system server which is not authorized
        // or with old protocol version
        m_items.append(new Item(server));
    }

    endResetModel();
    updateLowestInstalledVersion();
}

void QnServerUpdatesModel::updateVersionColumn() {
    if (m_items.isEmpty())
        return;
    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}

void QnServerUpdatesModel::updateLowestInstalledVersion()
{
    QnSoftwareVersion result;
    for (const auto item: m_items)
    {
        const auto& server = item->server();
        NX_ASSERT(server);
        if (!server)
            continue;

        const auto status = server->getStatus();
        if (status == Qn::Offline || status == Qn::Unauthorized)
            continue;

        const auto& version = server->getVersion();
        if (version.isNull())
            continue;

        if (version < result || result.isNull())
            result = version;
    }

    if (result == m_lowestInstalledVersion)
        return;

    m_lowestInstalledVersion = result;
    emit lowestInstalledVersionChanged();
}

void QnServerUpdatesModel::at_resourceAdded(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (server->hasFlags(Qn::fake_server)
        && !helpers::serverBelongsToCurrentSystem(server))
    {
        return;
    }

    int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(new Item(server));
    endInsertRows();
    updateLowestInstalledVersion();
}

void QnServerUpdatesModel::at_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QModelIndex idx = index(server->getId());
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());
    m_items.removeAt(idx.row());
    endRemoveRows();
    updateLowestInstalledVersion();
}

void QnServerUpdatesModel::at_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QModelIndex idx = index(server);
    bool exists = idx.isValid();
    bool isOurServer = !server->hasFlags(Qn::fake_server)
        || helpers::serverBelongsToCurrentSystem(server);

    if (exists == isOurServer) {
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
        updateLowestInstalledVersion();
        return;
    }

    if (isOurServer)
        at_resourceAdded(resource);
    else
        at_resourceRemoved(resource);
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

QnSoftwareVersion QnServerUpdatesModel::lowestInstalledVersion() const
{
    return m_lowestInstalledVersion;
}

QnServerUpdatesColors QnServerUpdatesModel::colors() const
{
    return m_colors;
}

void QnServerUpdatesModel::setColors(const QnServerUpdatesColors &colors) {
    m_colors = colors;

    if (m_items.isEmpty())
        return;
    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}
