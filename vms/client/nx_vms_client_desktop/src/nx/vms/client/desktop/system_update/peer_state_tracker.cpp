#include "peer_state_tracker.h"

#include <nx_ec/ec_proto_version.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/system_helpers.h>
#include <nx/utils/app_info.h>
#include <ui/workbench/workbench_context.h>
#include "client_update_tool.h"

namespace nx::vms::client::desktop {

PeerStateTracker::PeerStateTracker(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{

}

void PeerStateTracker::setResourceFeed(QnResourcePool* pool)
{
    QObject::disconnect(m_onAddedResource);
    QObject::disconnect(m_onRemovedResource);

    for (auto item: m_items)
        emit itemRemoved(item);
    m_items.clear();

    addItemForClient();
    const auto allServers = pool->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr &server : allServers)
        atResourceAdded(server);

    m_onAddedResource = connect(pool, &QnResourcePool::resourceAdded,
        this, &PeerStateTracker::atResourceAdded);
    m_onRemovedResource = connect(pool, &QnResourcePool::resourceRemoved,
        this, &PeerStateTracker::atResourceRemoved);
}

UpdateItemPtr PeerStateTracker::findItemById(QnUuid id)
{
    for (auto item: m_items)
    {
        if (item->id == id)
            return item;
    }
    return nullptr;
}

UpdateItemPtr PeerStateTracker::findItemByRow(int row) const
{
    if (row < 0 || row >= m_items.size())
        return nullptr;
    return m_items[row];
}

int PeerStateTracker::peersCount() const
{
    return m_items.size();
}

QnMediaServerResourcePtr PeerStateTracker::getServer(const UpdateItem* item) const
{
    if (!item)
        return QnMediaServerResourcePtr();
    return resourcePool()->getResourceById<QnMediaServerResource>(item->id);
}

nx::utils::SoftwareVersion PeerStateTracker::lowestInstalledVersion()
{
    nx::utils::SoftwareVersion result;
    for (const auto& item: m_items)
    {
        const auto& server = getServer(item.get());
        if (server)
        {
            const auto status = server->getStatus();
            if (status == Qn::Offline || status == Qn::Unauthorized)
                continue;
        }

        if (item->version.isNull())
            continue;

        if (item->version < result || result.isNull())
            result = item->version;
    }

    return result;
}

void PeerStateTracker::setUpdateTarget(const nx::utils::SoftwareVersion& version)
{
    m_targetVersion = version;
}

void PeerStateTracker::setUpdateStatus(const std::map<QnUuid, nx::update::Status>& statusAll)
{
    for (const auto& status: statusAll)
    {
        if (auto item = findItemById(status.first))
        {
            item->progress = status.second.progress * 100;
            item->statusMessage = status.second.message;
            item->state = status.second.code;
            if (item->state == StatusCode::latestUpdateInstalled && item->installing)
                item->installing = false;
            item->offline = (status.second.code == StatusCode::offline);
            //QModelIndex idx = index(item->row, 0);
            //emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
            emit itemChanged(item);
        }
    }
}

void PeerStateTracker::setPeersInstalling(QSet<QnUuid> targets, bool installing)
{
    for (const auto& uid: targets)
    {
        if (auto item = findItemById(uid))
        {
            if (item->installing == installing)
                continue;
            if (item->state == StatusCode::latestUpdateInstalled)
                continue;
            item->installing = installing;
            emit itemChanged(item);
        }
    }
}

void PeerStateTracker::clearState()
{
    for (auto& item: m_items)
    {
        item->state = StatusCode::idle;
        item->progress = 0;
        item->statusMessage = "Waiting for data";
        emit itemChanged(item);
    }
}

std::map<QnUuid, nx::update::Status::Code> PeerStateTracker::getAllPeerStates() const
{
    std::map<QnUuid, nx::update::Status::Code> result;

    for (const auto& item: m_items)
    {
        result.insert(std::make_pair(item->id, item->state));
    }
    return result;
}

QList<UpdateItemPtr> PeerStateTracker::getAllItems() const
{
    return m_items;
}

QSet<QnUuid> PeerStateTracker::getAllPeers() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getServersInState(StatusCode state) const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->state == state)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::getOfflineServers() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->offline)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getLegacyServers() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!getServer(item.get()))
            continue;
        if (item->onlyLegacyUpdate)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersInstalling() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installing)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersCompleteInstall() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installed)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getServersWithChangedProtocol() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!getServer(item.get()))
            continue;
        if (item->changedProtocol)
            result.insert(item->id);
    }
    return result;
}

void PeerStateTracker::atResourceAdded(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    const auto status = server->getStatus();
    if (status == Qn::Offline || status == Qn::Unauthorized)
        return;

    if (server->hasFlags(Qn::fake_server)
        && !helpers::serverBelongsToCurrentSystem(server))
    {
        return;
    }


    //int row = m_items.size();
    //beginInsertRows(QModelIndex(), row, row);
    auto item = addItemForServer(server);
    updateContentsIndex();
    emit itemAdded(item);
    //endInsertRows();

}

void PeerStateTracker::atResourceRemoved(const QnResourcePtr& resource)
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

    disconnect(server.data(), &QnResource::statusChanged,
        this, &PeerStateTracker::atResourceChanged);
    disconnect(server.data(), &QnMediaServerResource::versionChanged,
        this, &PeerStateTracker::atResourceChanged);
    disconnect(server.data(), &QnResource::flagsChanged,
        this, &PeerStateTracker::atResourceChanged);

    //QModelIndex idx = createIndex(item->row, 0);
    //if (!idx.isValid())
    //    return;

    m_items.removeAt(item->row);
    updateContentsIndex();

    emit itemRemoved(item);
}

void PeerStateTracker::atResourceChanged(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    auto item = findItemById(resource->getId());
    if (!item)
        return;
    updateServerData(server, item);
}

void PeerStateTracker::atClientupdateStateChanged(int state, int percentComplete)
{
    NX_ASSERT(m_clientItem);
    using State = ClientUpdateTool::State;

    m_clientItem->installing = false;
    m_clientItem->installed = false;
    m_clientItem->progress = 0;

    switch (State(state))
    {
        case State::initial:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = "Client has no update task";
            break;
        case State::readyDownload:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = "Client is ready to download update";
            break;
        case State::downloading:
            m_clientItem->state = StatusCode::downloading;
            m_clientItem->progress = percentComplete;
            m_clientItem->statusMessage = "Client is downloading an update";
            break;
        case State::readyInstall:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = "Client is ready to download update";
            break;
        case State::installing:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installing = true;
            m_clientItem->statusMessage = "Client is installing update";
            break;
        case State::complete:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installing = false;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = "Client has installed an update";
            break;
        case State::error:
            m_clientItem->state = StatusCode::error;
            m_clientItem->statusMessage = "Client has failed to download an update";
            break;
        case State::applauncherError:
            m_clientItem->state = StatusCode::error;
            m_clientItem->statusMessage = "Client has failed to install an update";
            break;
        default:
            break;
    }

    emit itemChanged(m_clientItem);
}

UpdateItemPtr PeerStateTracker::addItemForServer(QnMediaServerResourcePtr server)
{
    NX_ASSERT(server);
    if (!server)
        return nullptr;
    UpdateItemPtr item = std::make_shared<UpdateItem>();
    item->id = server->getId();
    item->component = UpdateItem::Component::server;
    item->version = server->getVersion();
    item->row = m_items.size();
    m_items.push_back(item);
    connect(server.data(), &QnResource::statusChanged,
        this, &PeerStateTracker::atResourceChanged);
    connect(server.data(), &QnMediaServerResource::versionChanged,
        this, &PeerStateTracker::atResourceChanged);
    connect(server.data(), &QnResource::flagsChanged,
        this, &PeerStateTracker::atResourceChanged);
    updateServerData(server, item);
    return item;
}

UpdateItemPtr PeerStateTracker::addItemForClient()
{
    UpdateItemPtr item = std::make_shared<UpdateItem>();
    item->id = commonModule()->globalSettings()->localSystemId();
    item->component = UpdateItem::Component::client;
    item->row = m_items.size();
    m_clientItem = item;
    m_items.push_back(item);
    updateClientData();
    return m_clientItem;
}

void PeerStateTracker::updateServerData(QnMediaServerResourcePtr server, UpdateItemPtr item)
{
    bool changed = false;
    auto status = server->getStatus();
    // TODO: Right now 'Incompatible' means we should use legacy update system to deal with them
    bool incompatible = status == Qn::ResourceStatus::Incompatible;
    if (incompatible != item->onlyLegacyUpdate)
    {
        item->onlyLegacyUpdate = incompatible;
        changed = true;
    }

    const nx::utils::SoftwareVersion newUpdateSupportVersion(4, 0);

    const auto& version = server->getVersion();
    if (version < newUpdateSupportVersion && !item->onlyLegacyUpdate)
    {
        item->onlyLegacyUpdate = true;
        changed = true;
    }

    if (version != item->version)
    {
        item->version = version;
        changed = true;
    }

    bool installed = (version == m_targetVersion);
    if (installed != item->installed)
    {
        item->installed = true;
        changed = true;
    }

    auto moduleInfo = server->getModuleInformation();
    bool changedProtocol = moduleInfo.protoVersion != nx_ec::EC2_PROTO_VERSION;
    if (item->changedProtocol !=  changedProtocol)
    {
        item->changedProtocol = changedProtocol;
        changed = true;
    }

    if (changed)
        emit itemChanged(item);
}

void PeerStateTracker::updateClientData()
{
    bool changed = false;
    auto version = nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion());
    if (m_clientItem->version != version)
    {
        m_clientItem->version = version;
        changed = true;
    }

    if (changed)
        emit itemChanged(m_clientItem);
}

void PeerStateTracker::updateContentsIndex()
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        m_items[i]->row = i;
    }
}

} // namespace nx::vms::client::desktop
