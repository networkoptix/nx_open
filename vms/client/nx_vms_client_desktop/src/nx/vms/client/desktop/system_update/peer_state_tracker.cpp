#include "peer_state_tracker.h"

#include <nx_ec/ec_proto_version.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/system_helpers.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ini.h>
#include <ui/workbench/workbench_context.h>
#include "client_update_tool.h"

namespace  {
    auto kWaitForServerReturn = std::chrono::minutes(10);
}

namespace nx::vms::client::desktop {

PeerStateTracker::PeerStateTracker(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    if(ini().massSystemUpdateWaitForServerOnline)
        m_waitForServerReturn = std::chrono::seconds(ini().massSystemUpdateWaitForServerOnline);
    else
        m_waitForServerReturn = kWaitForServerReturn;
}

bool PeerStateTracker::setResourceFeed(QnResourcePool* pool)
{
    QObject::disconnect(m_onAddedResource);
    QObject::disconnect(m_onRemovedResource);

    /// Reversing item list just to make sure we remove rows from the table from last to first.
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
        emit itemRemoved(*it);
    if (m_clientItem)
    {
        emit itemRemoved(m_clientItem);
        m_clientItem.reset();
    }

    m_items.clear();
    m_activeServers.clear();

    if (!pool)
    {
        NX_DEBUG(this, "setResourceFeed() got nullptr resource pool");
        return false;
    }

    auto systemId = helpers::currentSystemLocalId(commonModule());
    if (systemId.isNull())
    {
        NX_DEBUG(this, "setResourceFeed() got null system id");
        return false;
    }

    NX_DEBUG(this, "setResourceFeed() attaching to resource pool. Current systemId=%1", systemId);

    addItemForClient();

    const auto allServers = pool->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr& server: allServers)
        atResourceAdded(server);

    // We will not work with incompatible systems for now. We are going to deal with it in 4.1.
    //const auto incompatible = pool->getIncompatibleServers();
    //for (const QnMediaServerResourcePtr& server: incompatible)
    //    atResourceAdded(server);

    m_onAddedResource = connect(pool, &QnResourcePool::resourceAdded,
        this, &PeerStateTracker::atResourceAdded);
    m_onRemovedResource = connect(pool, &QnResourcePool::resourceRemoved,
        this, &PeerStateTracker::atResourceRemoved);
    return true;
}

UpdateItemPtr PeerStateTracker::findItemById(QnUuid id) const
{
    for (const auto& item: m_items)
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

QnMediaServerResourcePtr PeerStateTracker::getServer(const UpdateItemPtr& item) const
{
    if (!item)
        return QnMediaServerResourcePtr();
    if (item->incompatible)
        return resourcePool()->getIncompatibleServerById(item->id);
    return resourcePool()->getResourceById<QnMediaServerResource>(item->id);
}

QnMediaServerResourcePtr PeerStateTracker::getServer(QnUuid id) const
{
    return getServer(findItemById(id));
}

QnUuid PeerStateTracker::getClientPeerId() const
{
    NX_ASSERT(m_clientItem);
    return m_clientItem->id;
}

nx::utils::SoftwareVersion PeerStateTracker::lowestInstalledVersion()
{
    nx::utils::SoftwareVersion result;
    for (const auto& item: m_items)
    {
        const auto& server = getServer(item);
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
    NX_ASSERT(!version.isNull());
    m_targetVersion = version;

    for (auto& item: m_items)
    {
        if (version != item->version)
        {
            bool installed = (item->version == m_targetVersion)
                || item->state == StatusCode::latestUpdateInstalled;

            if (installed != item->installed)
            {
                item->installed = true;
                NX_INFO(this, "setUpdateTarget() - peer %1 changed installed=%2", item->id, item->installed);
            }
            emit itemChanged(item);
        }
    }
}

void PeerStateTracker::setVerificationError(const QSet<QnUuid>& targets, const QString& message)
{
    for (auto& item: m_items)
    {
        if (!targets.contains(item->id))
            continue;
        item->verificationMessage = message;
    }
}

void PeerStateTracker::setVerificationError(const QMap<QnUuid, QString>& errors)
{
    for (auto& item: m_items)
    {
        if (auto it = errors.find(item->id); it != errors.end())
            item->verificationMessage = it.value();
    }
}

void PeerStateTracker::clearVerificationErrors()
{
    for (auto& item: m_items)
        item->verificationMessage = QString();
}

bool PeerStateTracker::hasVerificationErrors() const
{
    for (auto& item: m_items)
    {
        if (!item->verificationMessage.isEmpty())
            return true;
    }
    return false;
}

bool PeerStateTracker::hasStatusErrors() const
{
    for (auto& item: m_items)
    {
        if (item->state == StatusCode::error && !item->statusMessage.isEmpty())
            return true;
    }
    return false;
}

void PeerStateTracker::setUpdateStatus(const std::map<QnUuid, nx::update::Status>& statusAll)
{
    for (const auto& status: statusAll)
    {
        if (auto item = findItemById(status.first))
        {
            item->progress = status.second.progress;
            item->statusMessage = status.second.message;
            item->state = status.second.code;
            if (item->state == StatusCode::latestUpdateInstalled && item->installing)
                item->installing = false;
            if (item->statusUnknown)
            {
                NX_DEBUG(this, "clearing unknown status for peer %1", item->id);
                item->statusUnknown = false;
            }
            // Ignoring 'offline' status from /ec2/updateStatus.
            //item->offline = (status.second.code == StatusCode::offline);
            emit itemChanged(item);
        }
    }
}

void PeerStateTracker::markStatusUnknown(const QSet<QnUuid>& targets)
{
    for (const auto& uid: targets)
    {
        if (auto item = findItemById(uid))
        {
            // Only server's state can be 'unknown'.
            if (item->component == UpdateItem::Component::server)
                item->statusUnknown = true;
        }
    }
}

void PeerStateTracker::setVersionInformation(
    const QList<nx::vms::api::ModuleInformation>& moduleInformation)
{
    for (const auto& info: moduleInformation)
    {
        if (auto item = findItemById(info.id))
        {
            if (info.version != item->version)
            {
                item->version = info.version;

                bool installed = (info.version == m_targetVersion)
                    || item->state == StatusCode::latestUpdateInstalled;

                if (installed != item->installed)
                {
                    item->installed = true;
                    NX_INFO(this, "setVersionInformation() - peer %1 changed installed=%2", item->id, item->installed);
                }
                emit itemChanged(item);
            }
        }
    }
}

void PeerStateTracker::setPeersInstalling(const QSet<QnUuid>& targets, bool installing)
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
        item->statusMessage = "Waiting for peer data";
        item->verificationMessage = "";
        item->installing = false;
        emit itemChanged(item);
    }
}

std::map<QnUuid, nx::update::Status::Code> PeerStateTracker::getAllPeerStates() const
{
    QnMutexLocker locker(&m_dataLock);
    std::map<QnUuid, nx::update::Status::Code> result;

    for (const auto& item: m_items)
        result.emplace(item->id, item->state);
    return result;
}

std::map<QnUuid, QnMediaServerResourcePtr> PeerStateTracker::getActiveServers() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_activeServers;
}

QList<UpdateItemPtr> PeerStateTracker::getAllItems() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_items;
}

QSet<QnUuid> PeerStateTracker::getAllPeers() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersInState(StatusCode state) const
{
    QnMutexLocker locker(&m_dataLock);
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
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->offline)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getLegacyServers() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!getServer(item))
            continue;
        if (item->onlyLegacyUpdate)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersInstalling() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installing && !item->installed)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersCompleteInstall() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installed || item->state == StatusCode::latestUpdateInstalled)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::getServersWithChangedProtocol() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!getServer(item))
            continue;
        if (item->changedProtocol)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::getPeersWithUnknownStatus() const
{
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->statusUnknown && !item->offline)
            result.insert(item->id);
    }
    return result;
}

void PeerStateTracker::processDownloadTaskSet()
{
    NX_ASSERT(!m_peersIssued.isEmpty());
    auto now = UpdateItem::Clock::now();

    for (const auto& item: m_items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (!m_peersIssued.contains(id))
            continue;

        if (item->statusUnknown)
            continue;

        if (item->incompatible)
            continue;

        if (item->offline || state == StatusCode::offline)
        {
            auto delta = now - item->lastTimeOnline;
            if (delta > m_waitForServerReturn)
            {
                NX_VERBOSE(this, "processDownloadTaskSet() "
                    "peer %1 has been offline for too long. Skipping it.", id);
                m_peersActive.remove(id);
                m_peersIssued.remove(id);
            }
        }
        else if (item->installed)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this, "processDownloadTaskSet() ",
                    "peer %1 has already installed this package.", id);
                m_peersActive.remove(id);
            }
            m_peersComplete.insert(id);
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this, "processDownloadTaskSet() "
                            "peer %1 completed downloading and is ready to install", id);
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
                case StatusCode::error:
                case StatusCode::idle:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this, "processDownloadTaskSet() "
                            "peer %1 failed to download update package", id);
                        m_peersActive.remove(id);
                    }
                    m_peersFailed.insert(id);
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                    if (!m_peersActive.contains(id) && m_peersIssued.contains(id))
                    {
                        NX_VERBOSE(this, "processDownloadTaskSet() "
                            "peer %1 has resumed downloading.", id);
                        m_peersActive.insert(id);
                    }
                    break;
                case StatusCode::offline:
                    // We should have already handled this case.
                    NX_ASSERT(false);
                    break;
                case StatusCode::latestUpdateInstalled:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this, "processDownloadTaskSet() "
                            "peer %1 has already installed this package.", id);
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
            }
        }
    }
}

void PeerStateTracker::processInstallTaskSet()
{
    NX_ASSERT(!m_peersIssued.isEmpty());

    QSet<QnUuid> peersToRestartUpdate;
    QSet<QnUuid> peersActive;
    QSet<QnUuid> readyToInstall;

    for (const auto& item: m_items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (item->incompatible)
            continue;
        //if (!m_peersIssued.contains(id))
        //    continue;
        if (item->installed)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this,
                    "processInstallTaskSet() - peer %1 has already installed this package.", id);
                m_peersActive.remove(id);
            }
            m_peersComplete.insert(id);
        }
        else if (item->installing)
        {
            if (!m_peersActive.contains(id))
            {
                NX_VERBOSE(this)
                    << "processInstallTaskSet() - peer"
                    << id << "has resumed installation";
                m_peersActive.insert(id);
                peersToRestartUpdate.insert(id);
            }
            if (item->component == UpdateItem::Component::server)
            {
                peersActive.insert(item->id);
            }
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processInstallTaskSet() - peer "
                            << id << "completed downloading and is ready to install";
                        m_peersActive.remove(id);
                        // TODO: Should force installing for this peers, until it is a client.
                        // Client should wait until servers have completed installation.
                        peersToRestartUpdate.insert(id);
                    }
                    readyToInstall.insert(id);
                    break;
                case StatusCode::error:
                case StatusCode::idle:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processInstallTaskSet() - peer"
                            << id << "failed to download update package";
                        m_peersFailed.insert(id);
                        m_peersActive.remove(id);
                    }
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                    if (!m_peersActive.contains(id)/* && m_peersIssued.contains(id)*/)
                    {
                        NX_VERBOSE(this)
                            << "processInstallTaskSet() - peer"
                            << id << "resumed downloading.";
                        m_peersActive.insert(id);
                    }
                    break;
                case StatusCode::latestUpdateInstalled:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this)
                            << "processInstallTaskSet() - peer"
                            << id << "have already installed this package.";
                        m_peersActive.remove(id);
                    }
                    m_peersComplete.insert(id);
                    break;
                case StatusCode::offline:
                    break;
            }
        }
    }
}

QSet<QnUuid> PeerStateTracker::getPeersComplete() const
{
    return m_peersComplete;
}

QSet<QnUuid> PeerStateTracker::getPeersActive() const
{
    return m_peersActive;
}

QSet<QnUuid> PeerStateTracker::getPeersIssued() const
{
    return m_peersIssued;
}

QSet<QnUuid> PeerStateTracker::getPeersFailed() const
{
    return m_peersFailed;
}

void PeerStateTracker::setTaskSet(const QSet<QnUuid> &targets)
{
    m_peersActive = targets;
    m_peersIssued = targets;
    m_peersComplete = {};
    m_peersFailed = {};
}

void PeerStateTracker::setTaskError(const QSet<QnUuid>& targets, const QString& /*error*/)
{
    for (const auto& id: targets)
    {
        if (auto item = findItemById(id))
            m_peersFailed.insert(id);
    }
}

void PeerStateTracker::atResourceAdded(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (!helpers::serverBelongsToCurrentSystem(server))
    {
        //auto systemId = helpers::currentSystemLocalId(commonModule());
        //NX_VERBOSE(this, "atResourceAdded(%1) server does not belong to the system %2",
        //     server->getName(), systemId);
        return;
    }

    //NX_VERBOSE(this, "atResourceAdded(%1)", resource->getName());
    const auto status = server->getStatus();
    if (status == Qn::Unauthorized)
    {
        NX_VERBOSE(this, "atResourceAdded(%1) - unauthorized", server->getName());
        return;
    }

    if (status == Qn::Incompatible)
    {
        NX_VERBOSE(this, "atResourceAdded(%1) - incompatible", server->getName());
        return;
    }

    UpdateItemPtr item;
    {
        QnMutexLocker locker(&m_dataLock);
        m_activeServers[server->getId()] = server;
        item = addItemForServer(server);
        updateServerData(server, item);
    }

    updateContentsIndex();
    emit itemAdded(item);
}

void PeerStateTracker::atResourceRemoved(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    server->disconnect(this);
    auto id = server->getId();
    auto item = findItemById(id);
    if (!item)
        return;

    // We should emit this event before m_items size is changed
    emit itemRemoved(item);
    {
        QnMutexLocker locker(&m_dataLock);
        m_activeServers.erase(id);
        m_peersIssued.remove(id);
        m_peersFailed.remove(id);
        m_peersActive.remove(id);
        m_items.removeAt(item->row);
        updateContentsIndex();
    }
}

void PeerStateTracker::atResourceChanged(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    NX_VERBOSE(this, "atResourceChanged(%1)", resource->getName());
    bool changed = false;

    UpdateItemPtr item;
    {
        QnMutexLocker locker(&m_dataLock);
        item = findItemById(resource->getId());
        if (!item)
            return;
        changed = updateServerData(server, item);
    }
    if (changed)
        emit itemChanged(item);
}

void PeerStateTracker::atClientupdateStateChanged(int state, int percentComplete)
{
    NX_ASSERT(m_clientItem);
    using State = ClientUpdateTool::State;
    NX_VERBOSE(this, "atClientupdateStateChanged(%1, %2)",
        ClientUpdateTool::toString(State(state)), percentComplete);

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
        case State::readyRestart:
            m_clientItem->statusMessage = "Client is ready to install and restart";
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            break;
        case State::installing:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installing = true;
            m_clientItem->statusMessage = "Client is installing update";
            break;
        case State::complete:
            m_clientItem->state = StatusCode::latestUpdateInstalled;
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
    NX_VERBOSE(this, "addItemForServer(%1)", server->getName());
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
    return item;
}

UpdateItemPtr PeerStateTracker::addItemForClient()
{
    UpdateItemPtr item = std::make_shared<UpdateItem>();

    // This ID is much more easy to distinguish.
    if (ini().massSystemUpdateDebugInfo)
        item->id = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");
    else
        item->id = commonModule()->globalSettings()->localSystemId();
    item->component = UpdateItem::Component::client;
    item->row = m_items.size();
    NX_VERBOSE(this, "addItemForClient() id=%1", item->id);
    m_clientItem = item;
    m_items.push_back(item);
    updateClientData();
    emit itemAdded(m_clientItem);
    return m_clientItem;
}

bool PeerStateTracker::updateServerData(QnMediaServerResourcePtr server, UpdateItemPtr item)
{
    bool changed = false;
    auto status = server->getStatus();

    bool incompatible = status == Qn::ResourceStatus::Incompatible
        || server->hasFlags(Qn::fake_server);

    // We should not get incompatible servers here. They are should be filtered out before.
    NX_ASSERT(!incompatible);
    if (incompatible != item->onlyLegacyUpdate)
    {
        item->onlyLegacyUpdate = incompatible;
        changed = true;
    }

    if (incompatible != item->incompatible)
    {
        item->incompatible = incompatible;
        changed = true;
    }

    const nx::utils::SoftwareVersion kNewUpdateSupportVersion(4, 0);

    const auto& version = server->getVersion();
    if (version < kNewUpdateSupportVersion && !item->onlyLegacyUpdate)
    {
        item->onlyLegacyUpdate = true;
        changed = true;
    }

    bool viewAsOffline = !server->isOnline() || incompatible;
    if (item->offline != viewAsOffline)
    {
        if (viewAsOffline)
        {
            // TODO: Should track online->offline changes for task sets
            item->lastTimeOnline = UpdateItem::Clock::now();
            item->statusUnknown = false;
        }
        else
        {
            // TODO: Should track offline->online changes for task sets:
            //  - peersActive
            //  - peersFailed
        }
        item->offline = viewAsOffline;
        changed = true;
    }

    if (version != item->version)
    {
        NX_INFO(this, "updateServerData() - peer %1 changing version from=%2 to %3", item->id, item->version, version);
        item->version = version;
        changed = true;
    }

    bool installed = (version == m_targetVersion) || item->state == StatusCode::latestUpdateInstalled;
    if (installed != item->installed)
    {
        item->installed = true;
        NX_INFO(this, "updateServerData() - peer %1 changed installed=%2", item->id, item->installed);
        changed = true;
    }

    auto moduleInfo = server->getModuleInformation();
    bool changedProtocol = moduleInfo.protoVersion != nx_ec::EC2_PROTO_VERSION;
    if (item->changedProtocol !=  changedProtocol)
    {
        item->changedProtocol = changedProtocol;
        changed = true;
    }

    return changed;
}

bool PeerStateTracker::updateClientData()
{
    bool changed = false;
    auto version = nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion());
    if (m_clientItem->version != version)
    {
        m_clientItem->version = version;
        changed = true;
    }

    return changed;
}

void PeerStateTracker::updateContentsIndex()
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        m_items[i]->row = i;
    }
}

} // namespace nx::vms::client::desktop
