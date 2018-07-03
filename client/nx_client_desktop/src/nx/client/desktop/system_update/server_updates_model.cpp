#include "server_updates_model.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <QtWidgets/qapplication.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

namespace nx {
namespace client {
namespace desktop {

ServerUpdatesModel::ServerUpdatesModel(QObject* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Dat strange thingy.
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(),
        &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged, this, &ServerUpdatesModel::updateVersionColumn);
}

void ServerUpdatesModel::setResourceFeed(QnResourcePool* pool)
{
    resetResourses(pool);

    connect(pool, &QnResourcePool::resourceAdded, this, &ServerUpdatesModel::at_resourceAdded);
    connect(pool, &QnResourcePool::resourceRemoved, this, &ServerUpdatesModel::at_resourceRemoved);
    connect(pool, &QnResourcePool::resourceChanged, this, &ServerUpdatesModel::at_resourceChanged);
    connect(pool, &QnResourcePool::statusChanged, this, &ServerUpdatesModel::at_resourceChanged);
}

int ServerUpdatesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int ServerUpdatesModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;

    return m_items.size();
}

QSet<QnUuid> ServerUpdatesModel::getAllServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_items)
    {
        if (!item->server)
            continue;
        result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdatesModel::getServersInState(StatusCode state) const
{
    QSet<QnUuid> result;
    for (const auto& item : m_items)
    {
        if (!item->server)
            continue;
        if (item->state == state)
            result.insert(item->server->getId());
    }
    return result;
}

UpdateItemPtr ServerUpdatesModel::findItemById(QnUuid id)
{
    for (auto item: m_items)
    {
        if (item->server->getId() == id)
            return item;
    }
    return nullptr;
}

// Get servers that are offline right now
QSet<QnUuid> ServerUpdatesModel::getOfflineServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_items)
    {
        if (!item->server)
            continue;
        if (item->offline)
            result.insert(item->server->getId());
    }
    return result;
}
// Get servers that are incompatible with new update system
QSet<QnUuid> ServerUpdatesModel::getLegacyServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_items)
    {
        if (!item->server)
            continue;
        if (item->onlyLegacyUpdate)
            result.insert(item->server->getId());
    }
    return result;
}

UpdateItemPtr ServerUpdatesModel::findItemByRow(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return m_items[row];
}

void ServerUpdatesModel::setUpdateStatus(const std::map<QnUuid, nx::api::Updates2StatusData>& statusAll)
{
    for (const auto& status: statusAll)
    {
        if (auto item = findItemById(status.first))
        {
            item->progress = status.second.progress;
            item->statusMessage = status.second.message;
            item->state = status.second.state;
            QModelIndex idx = index(item->row, 0);
            emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
        }
    }
}

QVariant ServerUpdatesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case NameColumn:
                return tr("Server");
            case VersionColumn:
                return tr("Current Version");
            case ProgressColumn:
                return tr("Status");
            case StatusColumn:
                return tr("Message");
            default:
                break;
        }
    }
    return QVariant();
}

QVariant ServerUpdatesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    auto item = findItemByRow(index.row());

    if (!item)
        return QVariant();

    int column = index.column();

    if (role == Qt::ForegroundRole)
    {
        if (column == VersionColumn)
        {
            if (item->offline)
                return qApp->palette().color(QPalette::Button);

            if (m_latestVersion <= item->server->getVersion())
                return m_versionColors.latest;

            if (m_updatePlatforms.contains(item->server->getSystemInfo()))
                return m_versionColors.target;

            //if (m_updateTargets.contains(item->server->getId()))
            //    return m_versionColors.target;

            return m_versionColors.error;
        }
        else if (column == NameColumn)
        {
            if (item->offline)
                return qApp->palette().color(QPalette::Button);
                //return m_versionColors.error;
        }
    }

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            switch (column)
            {
                case NameColumn:
                    return QnResourceDisplayInfo(item->server).toString(qnSettings->extraInfoInTree());
                case VersionColumn:
                    return item->server->getVersion().toString(nx::utils::SoftwareVersion::FullFormat);
                //case ProgressColumn:
                //    return item->progress;
                case StatusColumn:
                    return item->statusMessage;
                default:
                    break;
            }
            break;
        case Qt::DecorationRole:
            if (column == NameColumn)
                return qnResIconCache->icon(item->server);
            break;
        case Qn::MediaServerResourceRole:
            return QVariant::fromValue<QnMediaServerResourcePtr>(item->server);

        case UpdateItem::UpdateItemRole:
            return QVariant::fromValue(item);
        default:
            break;
    }
    return QVariant();
}

Qt::ItemFlags ServerUpdatesModel::flags(const QModelIndex& index) const
{
    if (index.column() == ProgressColumn)
        return Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return base_type::flags(index);
}

void ServerUpdatesModel::addItemForServer(QnMediaServerResourcePtr server)
{
    UpdateItemPtr item = std::make_shared<UpdateItem>();
    item->server = server;
    item->row = m_items.size();
    m_items.push_back(item);
    updateServerData(server, *item);
}

void ServerUpdatesModel::resetResourses(QnResourcePool* pool)
{
    beginResetModel();
    m_items.clear();

    const auto allServers = pool->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr &server : allServers)
        addItemForServer(server);

    for (const auto& server: pool->getIncompatibleServers())
    {
        if (!server || !helpers::serverBelongsToCurrentSystem(server))
            continue;

        // Adds newly added to system server which is not authorized
        // or with old protocol version
        addItemForServer(server);
    }

    endResetModel();
    updateContentsIndex();
}

void ServerUpdatesModel::updateContentsIndex()
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        m_items[i]->row = i;
    }
}

void ServerUpdatesModel::updateVersionColumn()
{
    if (m_items.empty())
        return;
    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}

nx::utils::SoftwareVersion ServerUpdatesModel::lowestInstalledVersion()
{
    nx::utils::SoftwareVersion result;
    for (const auto& item: m_items)
    {
        const auto& server = item->server;
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

    return result;
}

void ServerUpdatesModel::at_resourceAdded(const QnResourcePtr &resource)
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
    addItemForServer(server);
    endInsertRows();
    updateContentsIndex();
}

void ServerUpdatesModel::at_resourceRemoved(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    auto item = findItemById(server->getId());
    if (!item)
    {
        // Warning here
        return;
    }

    QModelIndex idx = createIndex(item->row, 0);
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());
    m_items.removeAt(idx.row());
    endRemoveRows();
    updateContentsIndex();
}

void ServerUpdatesModel::updateServerData(QnMediaServerResourcePtr server, UpdateItem& item)
{
    bool changed = false;

    QModelIndex idx = createIndex(item.row, 0);
    bool exists = idx.isValid();

    auto status = server->getStatus();
    bool offline = status == Qn::ResourceStatus::Offline;
    if (offline != item.offline)
    {
        item.offline = offline;
        changed = true;
    }

    // TODO: Right now 'Incompatible' means we should use legacy update system to deal with them
    bool incompatible = status == Qn::ResourceStatus::Incompatible;
    if (incompatible != item.onlyLegacyUpdate)
    {
        item.onlyLegacyUpdate = incompatible;
        changed = true;
    }

    const nx::utils::SoftwareVersion newUpdateSupportVersion(4, 0);

    const auto& version = server->getVersion();
    if (version < newUpdateSupportVersion && !item.onlyLegacyUpdate)
    {
        item.onlyLegacyUpdate = true;
        changed = true;
    }

    bool isOurServer = !server->hasFlags(Qn::fake_server)
        || helpers::serverBelongsToCurrentSystem(server);

    if (changed)
    {
        emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
        updateContentsIndex();
    }
}

void ServerUpdatesModel::at_resourceChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (auto item = findItemById(server->getId()))
        updateServerData(server, *item);
}

nx::utils::SoftwareVersion ServerUpdatesModel::latestVersion() const
{
    return m_latestVersion;
}

void ServerUpdatesModel::setUpdateTarget(const nx::utils::SoftwareVersion& version,
    const QSet<nx::vms::api::SystemInformation>& selection)
{
    m_latestVersion = version;
    m_updatePlatforms = selection;
    updateVersionColumn();
}

QnServerUpdatesColors ServerUpdatesModel::colors() const
{
    return m_versionColors;
}

void ServerUpdatesModel::setColors(const QnServerUpdatesColors& colors)
{
    m_versionColors = colors;

    if (m_items.empty())
        return;

    emit dataChanged(index(0, VersionColumn), index(m_items.size() - 1, VersionColumn));
}

} // namespace desktop
} // namespace client
} // namespace nx
