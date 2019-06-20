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

namespace {

std::chrono::milliseconds kWaitForServerReturn = std::chrono::minutes(10);

template<class Type> bool compareAndSet(const Type& from, Type& to)
{
    if (from == to)
        return false;
    to = from;
    return true;
}

} // anonymous namespace

namespace nx::vms::client::desktop {

PeerStateTracker::PeerStateTracker(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_dataLock(QnMutex::Recursive)
{
    if (ini().massSystemUpdateWaitForServerOnlineSecOverride)
    {
        m_timeForServerToReturn = std::chrono::seconds(
            ini().massSystemUpdateWaitForServerOnlineSecOverride);
    }
    else
    {
        m_timeForServerToReturn = kWaitForServerReturn;
    }
}

bool PeerStateTracker::setResourceFeed(QnResourcePool* pool)
{
    QObject::disconnect(m_onAddedResource);
    QObject::disconnect(m_onRemovedResource);

    auto itemsCache = m_items;
    /// Reversing item list just to make sure we remove rows from the table from last to first.
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
        emit itemToBeRemoved(*it);
    if (m_clientItem)
    {
        emit itemToBeRemoved(m_clientItem);
        m_clientItem.reset();
    }

    m_items.clear();
    m_activeServers.clear();

    for (auto it = itemsCache.rbegin(); it != itemsCache.rend(); ++it)
        emit itemRemoved(*it);

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
    NX_VERBOSE(this, "setUpdateTarget(%1)", version);
    m_targetVersion = version;

    // VMS-13789: Should set client version to target version, if client has installed it.
    if (m_clientItem && m_clientItem->state == StatusCode::latestUpdateInstalled)
    {
        m_clientItem->version = version;
    }

    for (auto& item: m_items)
    {
        if (version != item->version)
        {
            bool installed = (item->version == m_targetVersion)
                || item->state == StatusCode::latestUpdateInstalled;

            if (installed != item->installed)
            {
                item->installed = installed;
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

int PeerStateTracker::setUpdateStatus(const std::map<QnUuid, nx::update::Status>& statusAll)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        QnMutexLocker locker(&m_dataLock);
        for (const auto& status: statusAll)
        {
            if (auto item = findItemById(status.first))
            {
                bool changed = false;

                if (status.second.code == StatusCode::offline)
                    continue;

                QString name = item->component == UpdateItem::Component::client
                    ? QString("client")
                    : getServer(item)->getName();

                if (item->state != status.second.code)
                {
                    NX_INFO(this, "setUpdateStatus() - changing status %1->%2 for peer %3: %4",
                        toString(item->state), toString(status.second.code), name, item->id);
                    item->state = status.second.code;
                    changed = true;
                }

                changed |= compareAndSet(status.second.progress, item->progress);
                changed |= compareAndSet(status.second.message, item->statusMessage);
                //changed |= compareAndSet(status.second.code, item->state);

                if (item->state == StatusCode::latestUpdateInstalled && item->installing)
                {
                    NX_INFO(this, "setUpdateStatus() - removing installation status for %1",
                        item->id);
                    item->installing = false;
                    changed = true;
                }

                if (item->statusUnknown)
                {
                    NX_INFO(this,
                        "setUpdateStatus() - clearing unknown status for peer %1, status=%2",
                        item->id, toString(item->state));
                    item->statusUnknown = false;
                }

                item->lastStatusTime = UpdateItem::Clock::now();

                if (changed)
                    itemsChanged.append(item);
            }
        }
    }
    for (auto item: itemsChanged)
        emit itemChanged(item);
    return itemsChanged.size();
}

void PeerStateTracker::markStatusUnknown(const QSet<QnUuid>& targets)
{
    for (const auto& uid: targets)
    {
        if (auto item = findItemById(uid))
        {
            // Only server's state can be 'unknown'.
            if (item->component == UpdateItem::Component::server
                && (!item->offline || item->state != StatusCode::offline))
            {
                item->statusUnknown = true;
            }
        }
    }
}

void PeerStateTracker::setVersionInformation(
    const QList<nx::vms::api::ModuleInformation>& moduleInformation)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        QnMutexLocker locker(&m_dataLock);
        for (const auto& info: moduleInformation)
        {
            if (auto item = findItemById(info.id))
            {
                bool changed = false;

                changed |= compareAndSet<nx::utils::SoftwareVersion>(info.version, item->version);

                bool installed = (info.version == m_targetVersion)
                    || item->state == StatusCode::latestUpdateInstalled;

                if (installed != item->installed)
                {
                    item->installed = installed;
                    changed = true;
                    NX_INFO(this, "setVersionInformation() - peer %1 changed installed=%2",
                        item->id, item->installed);
                }

                if (item->protocol !=  info.protoVersion)
                {
                    NX_INFO(this, "setVersionInformation() - peer %1 changed protocol %2 to %3",
                        item->id, item->protocol, info.protoVersion);
                    item->protocol = info.protoVersion;
                    changed = true;
                }

                if (changed)
                    itemsChanged.append(item);
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::setPeersInstalling(const QSet<QnUuid>& targets, bool installing)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        QnMutexLocker locker(&m_dataLock);
        for (const auto& uid: targets)
        {
            if (auto item = findItemById(uid))
            {
                if (item->installing == installing)
                    continue;
                if (item->state == StatusCode::latestUpdateInstalled)
                    continue;
                item->installing = installing;
                itemsChanged.append(item);
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::clearState()
{
    NX_DEBUG(this, "clearState()");
    QnMutexLocker locker(&m_dataLock);
    for (auto& item: m_items)
    {
        // Offline peers are already 'clean' enough, ignoring them.
        if (item->state == StatusCode::offline && item->offline)
            continue;
        item->state = StatusCode::idle;
        item->progress = 0;
        item->statusMessage = "Waiting for peer data";
        item->verificationMessage = "";
        item->installing = false;
    }

    auto items = m_items;
    locker.unlock();

    for (auto item: m_items)
        emit itemChanged(item);
}

std::map<QnUuid, nx::update::Status::Code> PeerStateTracker::allPeerStates() const
{
    QnMutexLocker locker(&m_dataLock);
    std::map<QnUuid, nx::update::Status::Code> result;

    for (const auto& item: m_items)
        result.emplace(item->id, item->state);
    return result;
}

std::map<QnUuid, QnMediaServerResourcePtr> PeerStateTracker::activeServers() const
{
    QnMutexLocker locker(&m_dataLock);

    std::map<QnUuid, QnMediaServerResourcePtr> result;
    for (const auto& item: m_activeServers)
    {
        if (!item.second->isOnline())
            continue;
        result[item.first] = item.second;
    }
    return result;
}

QList<UpdateItemPtr> PeerStateTracker::allItems() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_items;
}

QSet<QnUuid> PeerStateTracker::allPeers() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::peersInState(StatusCode state) const
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

QSet<QnUuid> PeerStateTracker::offlineServers() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->offline)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::offlineNotTooLong() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->offline && item->state != StatusCode::offline)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::legacyServers() const
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

QSet<QnUuid> PeerStateTracker::peersInstalling() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installing && !item->installed)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::peersCompleteInstall() const
{
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installed || item->state == StatusCode::latestUpdateInstalled)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::serversWithChangedProtocol() const
{
    int protocol = nx_ec::EC2_PROTO_VERSION;
    QnMutexLocker locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!getServer(item))
            continue;
        if (item->protocol != protocol)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::peersWithUnknownStatus() const
{
    QSet<QnUuid> result;
    QnMutexLocker locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (item->statusUnknown && !item->offline)
            result.insert(item->id);
    }
    return result;
}

void PeerStateTracker::processUnknownStates()
{
    QList<UpdateItemPtr> itemsChanged;
    auto now = UpdateItem::Clock::now();

    {
        QnMutexLocker locker(&m_dataLock);
        for (const auto& item: m_items)
        {
            StatusCode state = item->state;
            auto id = item->id;

            if (item->statusUnknown && state != StatusCode::offline)
            {
                auto delta = now - item->lastStatusTime;
                if (delta > m_timeForServerToReturn)
                {
                    NX_INFO(this, "processUnknownStates() "
                        "peer %1 had no status updates for too long. Skipping it.", id);
                    item->state = StatusCode::error;
                    item->statusMessage = tr("The server is taking too long to respond");
                }
                continue;
            }

            if (item->offline && state != StatusCode::offline)
            {
                auto delta = now - item->lastOnlineTime;
                if (delta > m_timeForServerToReturn)
                {
                    NX_INFO(this, "processUnknownStates() "
                        "peer %1 has been offline for too long. Skipping it.", id);
                    item->state = StatusCode::offline;
                    itemsChanged.push_back(item);
                }
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::processDownloadTaskSet()
{
    NX_ASSERT(!m_peersIssued.isEmpty());
    QnMutexLocker locker(&m_dataLock);

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

        if (item->installed)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this, "processDownloadTaskSet() "
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
                    NX_VERBOSE(this, "processDownloadTaskSet() "
                        "peer %1 has been offline for too long. Skipping it.", id);
                    m_peersActive.remove(id);
                    m_peersIssued.remove(id);
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
                case StatusCode::idle:
                    // Most likely mediaserver did not have enough time to start downloading.
                    // Nothing to do here.
                    break;
            }
        }
    }
}

void PeerStateTracker::processInstallTaskSet()
{
    NX_ASSERT(!m_peersIssued.isEmpty());
    QnMutexLocker locker(&m_dataLock);

    for (const auto& item: m_items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (item->incompatible)
            continue;
        //if (!m_peersIssued.contains(id))
        //    continue;
        if (item->installed || state == StatusCode::latestUpdateInstalled)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this,
                    "processInstallTaskSet() - peer %1 has installed this package.", id);
                m_peersActive.remove(id);
            }
            m_peersComplete.insert(id);
        }
        else if (item->installing)
        {
            if (!m_peersActive.contains(id))
            {
                NX_VERBOSE(this,
                    "processInstallTaskSet() - peer %1 has resumed installation", id);
                m_peersActive.insert(id);
            }
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (m_peersActive.contains(id) && !m_peersFailed.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processInstallTaskSet() - peer %1 is not installing anything", id);
                        m_peersFailed.insert(id);
                    }
                    // Nothing to do here.
                    break;
                case StatusCode::error:
                case StatusCode::idle:
                    if (m_peersActive.contains(id))
                    {
                        NX_INFO(this,
                            "processInstallTaskSet() - peer %1 failed to download update package", id);
                        m_peersFailed.insert(id);
                        m_peersActive.remove(id);
                    }
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                    if (!m_peersActive.contains(id)/* && m_peersIssued.contains(id)*/)
                    {
                        NX_VERBOSE(this,
                            "processInstallTaskSet() - peer %1, resumed downloading.", id);
                        m_peersActive.insert(id);
                    }
                    break;
                case StatusCode::latestUpdateInstalled:
                    // We should have handled this state before.
                    NX_ASSERT(false);
                    break;
                case StatusCode::offline:
                    break;
            }
        }
    }
}

QSet<QnUuid> PeerStateTracker::peersComplete() const
{
    return m_peersComplete;
}

QSet<QnUuid> PeerStateTracker::peersActive() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_peersActive;
}

QSet<QnUuid> PeerStateTracker::peersIssued() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_peersIssued;
}

QSet<QnUuid> PeerStateTracker::peersFailed() const
{
    QnMutexLocker locker(&m_dataLock);
    return m_peersFailed;
}

void PeerStateTracker::setTask(const QSet<QnUuid> &targets)
{
    QnMutexLocker locker(&m_dataLock);
    m_peersActive = targets;
    m_peersIssued = targets;
    m_peersComplete = {};
    m_peersFailed = {};
}

void PeerStateTracker::setTaskError(const QSet<QnUuid>& targets, const QString& /*error*/)
{
    QnMutexLocker locker(&m_dataLock);
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
    emit itemToBeRemoved(item);
    {
        QnMutexLocker locker(&m_dataLock);
        m_activeServers.erase(id);
        m_peersIssued.remove(id);
        m_peersFailed.remove(id);
        m_peersActive.remove(id);
        m_items.removeAt(item->row);
        updateContentsIndex();
    }
    emit itemRemoved(item);
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
    using State = ClientUpdateTool::State;
    NX_ASSERT(m_clientItem);
    if (!m_clientItem)
    {
        NX_VERBOSE(this, "atClientupdateStateChanged(%1, %2) - client item is already destroyed",
            ClientUpdateTool::toString(State(state)), percentComplete);
        return;
    }

    NX_VERBOSE(this, "atClientupdateStateChanged(%1, %2)",
        ClientUpdateTool::toString(State(state)), percentComplete);

    m_clientItem->installing = false;
    m_clientItem->installed = false;
    m_clientItem->progress = 0;

    switch (State(state))
    {
        case State::initial:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = tr("No update task");
            break;
        case State::readyDownload:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = tr("Ready to download update");
            break;
        case State::downloading:
            m_clientItem->state = StatusCode::downloading;
            m_clientItem->progress = percentComplete;
            m_clientItem->statusMessage = tr("Downloading update");
            break;
        case State::readyInstall:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = tr("Ready to install update");
            break;
        case State::readyRestart:
            m_clientItem->statusMessage = tr("Ready to restart to the new version");
            m_clientItem->state = StatusCode::readyToInstall;
            NX_ASSERT(!m_targetVersion.isNull());
            m_clientItem->version = m_targetVersion;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            break;
        case State::installing:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installing = true;
            m_clientItem->statusMessage = tr("Installing update");
            break;
        case State::complete:
            m_clientItem->state = StatusCode::latestUpdateInstalled;
            NX_ASSERT(!m_targetVersion.isNull());
            m_clientItem->version = m_targetVersion;
            m_clientItem->installing = false;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = tr("Installed");
            break;
        case State::error:
            m_clientItem->state = StatusCode::error;
            m_clientItem->statusMessage = tr("Failed to download update");
            break;
        case State::applauncherError:
            m_clientItem->state = StatusCode::error;
            m_clientItem->statusMessage = tr("Failed to install update");
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
    NX_VERBOSE(this, "addItemForServer(name=%1, id=%2)", server->getName(), server->getId());
    UpdateItemPtr item = std::make_shared<UpdateItem>();
    item->id = server->getId();
    item->component = UpdateItem::Component::server;
    item->version = server->getVersion();
    item->row = m_items.size();
    auto info = server->getModuleInformation();
    item->protocol = info.protoVersion;

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
    item->protocol = nx_ec::EC2_PROTO_VERSION;
    NX_VERBOSE(this, "addItemForClient() id=%1", item->id);
    m_clientItem = item;
    m_items.push_back(item);
    updateClientData();
    emit itemAdded(m_clientItem);
    return m_clientItem;
}

bool PeerStateTracker::updateServerData(QnMediaServerResourcePtr server, UpdateItemPtr item)
{
    // NOTE: We should be wrapped by a mutex here.
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
            item->lastOnlineTime = UpdateItem::Clock::now();
            item->statusUnknown = false;
        }
        else
        {
            // TODO: Should track offline->online changes for task sets:
            //  - peersActive
            //  - peersFailed
        }
        item->offline = viewAsOffline;
        // TODO: Should take this away, out of the mutex scope.
        emit itemOnlineStatusChanged(item);
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
    if (moduleInfo.protoVersion != item->protocol)
    {
        NX_INFO(this, "updateServerData() - peer %1 has changed protocol version %1 to %2",
            item->id, item->protocol, moduleInfo.protoVersion);
        item->protocol = moduleInfo.protoVersion;
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
