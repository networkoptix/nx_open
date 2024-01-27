// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "peer_state_tracker.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/system_helpers.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

namespace {

std::chrono::milliseconds kWaitForServerReturn = std::chrono::minutes(10);

template<class Type> bool compareAndSet(const Type& from, Type& to)
{
    if (from == to)
        return false;
    to = from;
    return true;
}

const auto kDebugSampleUuid = QnUuid("cccccccc-cccc-cccc-cccc-cccccccccccc");

} // anonymous namespace

namespace nx::vms::client::desktop {

bool UpdateItem::isServer() const
{
    return component == Component::server;
}

bool UpdateItem::isClient() const
{
    return component == Component::client;
}

PeerStateTracker::PeerStateTracker(QObject* parent):
    base_type(parent),
    m_dataLock(nx::Mutex::Recursive)
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

void PeerStateTracker::setServerFilter(ServerFilter filter)
{
    m_serverFilter = filter;
}

bool PeerStateTracker::setResourceFeed(QnResourcePool* pool)
{
    QObject::disconnect(m_onAddedResource);
    QObject::disconnect(m_onRemovedResource);

    m_resourcePool = nullptr;

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

    for (auto it = itemsCache.rbegin(); it != itemsCache.rend(); ++it)
        emit itemRemoved(*it);

    if (!pool)
    {
        NX_DEBUG(this, "setResourceFeed() got nullptr resource pool");
        return false;
    }

    m_resourcePool = pool;

    NX_DEBUG(this, "setResourceFeed() attaching to resource pool.");

    addItemForClient(pool->systemContext());

    const auto allServers = pool->servers();
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
    if (!item || !m_resourcePool)
        return QnMediaServerResourcePtr();
    return m_resourcePool->getResourceById<QnMediaServerResource>(item->id);
}

QnMediaServerResourcePtr PeerStateTracker::getServer(QnUuid id) const
{
    return getServer(findItemById(id));
}

QnUuid PeerStateTracker::getClientPeerId(nx::vms::common::SystemContext* context) const
{
    // This ID is much more easy to distinguish.
    if (ini().massSystemUpdateDebugInfo)
        return kDebugSampleUuid;

    if (NX_ASSERT(context))
        return context->globalSettings()->localSystemId();

    return QnUuid();
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
            if (status == nx::vms::api::ResourceStatus::offline
                || status == nx::vms::api::ResourceStatus::unauthorized
                || status == nx::vms::api::ResourceStatus::mismatchedCertificate)
            {
                continue;
            }
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
    NX_VERBOSE(this, "setUpdateTarget(%1)", version);
    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
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
                itemsChanged << item;
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::setVerificationError(const QSet<QnUuid>& targets, const QString& message)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
        for (auto& item: m_items)
        {
            if (!targets.contains(item->id))
                continue;
            item->verificationMessage = message;
            itemsChanged << item;
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::setVerificationError(const QMap<QnUuid, QString>& errors)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
        for (auto& item: m_items)
        {
            if (auto it = errors.find(item->id); it != errors.end())
            {
                item->verificationMessage = it.value();
                itemsChanged << item;
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::clearVerificationErrors()
{
    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
        for (auto& item: m_items)
        {
            item->verificationMessage = QString();
            itemsChanged << item;
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

bool PeerStateTracker::hasVerificationErrors() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (auto& item: m_items)
    {
        if (!item->verificationMessage.isEmpty())
            return true;
    }
    return false;
}

bool PeerStateTracker::hasStatusErrors() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (auto& item: m_items)
    {
        if (item->state == StatusCode::error && !item->statusMessage.isEmpty())
            return true;
    }
    return false;
}

bool PeerStateTracker::getErrorReport(ErrorReport& report) const
{
    auto lastCode = UpdateItem::ErrorCode::noError;

    for (auto& item: m_items)
    {
        if (item->errorCode == UpdateItem::ErrorCode::noError)
            continue;

        auto errorCode = item->errorCode;
        if (errorCode < lastCode || lastCode == UpdateItem::ErrorCode::noError)
        {
            lastCode = errorCode;
            // Since all error codes are sorted, we can drop all peers with previous code.
            report.peers.clear();
        }

        if (errorCode == lastCode)
            report.peers.insert(item->id);
    }

    if (lastCode == UpdateItem::ErrorCode::noError)
        return false;

    report.message = errorString(lastCode);
    return true;
}

int PeerStateTracker::setUpdateStatus(const std::map<QnUuid, nx::vms::common::update::Status>& statusAll)
{
    auto getComponentName =
        [this](const UpdateItemPtr& item) -> QString
        {
            if (item->component == UpdateItem::Component::client)
                return "client";

            const auto server = getServer(item);
            return server
                ? server->getName()
                : "<Unknown server>";
        };

    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
        for (const auto& status: statusAll)
        {
            if (auto item = findItemById(status.first))
            {
                bool changed = false;

                item->lastStatusTime = UpdateItem::Clock::now();

                if (status.second.code == StatusCode::offline)
                    continue;

                if (item->state != status.second.code)
                {
                    auto newState = status.second.code;
                    if (status.second.code == nx::vms::common::update::Status::Code::error)
                    {
                        QString error = errorString(status.second.errorCode);
                        NX_INFO(this,
                            "setUpdateStatus() - changing status %1->error:\"%2\" for peer %3: %4",
                            toString(item->state),
                            error,
                            getComponentName(item),
                            item->id);
                    }
                    else if (item->state == nx::vms::common::update::Status::Code::offline
                        && item->incompatible
                        && (newState == nx::vms::common::update::Status::Code::idle
                            || newState == nx::vms::common::update::Status::Code::starting))
                    {
                        /**
                         * A special crutch to stop incompatible servers switching state
                         * to online/offline on every update.
                         */
                        newState = nx::vms::common::update::Status::Code::offline;
                    }
                    else
                    {
                        NX_INFO(this,
                            "setUpdateStatus() - changing status %1->%2 for peer %3: %4",
                            toString(item->state),
                            toString(status.second.code),
                            getComponentName(item),
                            item->id);
                    }

                    item->state = status.second.code;
                    changed = true;
                }

                changed |= compareAndSet(status.second.progress, item->progress);
                changed |= compareAndSet(status.second.message, item->debugMessage);
                changed |= compareAndSet(status.second.errorCode, item->errorCode);

                changed |= compareAndSet(
                    // -1 because we don't count an update package for the server itself.
                    (int) status.second.downloads.size()
                        - (status.second.mainUpdatePackage.isEmpty() ? 0 : 1),
                    item->offlinePackageCount);

                int offlinePackageProgress = 0;
                auto haveCorruptedDownloads = false;
                auto haveDownloadsInProgress = false;
                for (const common::update::PackageDownloadStatus& package: status.second.downloads)
                {
                    if (package.file == status.second.mainUpdatePackage)
                        continue;

                    offlinePackageProgress += package.downloadedBytes * 100 / package.size;
                    haveCorruptedDownloads |= (package.status == PackageStatusCode::corrupted);
                    haveDownloadsInProgress |= (package.status == PackageStatusCode::downloading);
                }

                changed |= compareAndSet(offlinePackageProgress, item->offlinePackageProgress);

                auto offlinePackageStatus = haveCorruptedDownloads
                    ? PackageStatusCode::corrupted
                    : haveDownloadsInProgress
                        ? PackageStatusCode::downloading
                        : PackageStatusCode::downloaded;
                changed |= compareAndSet(offlinePackageStatus, item->offlinePackagesStatus);

                if (status.second.code == StatusCode::error)
                {
                    NX_ASSERT(status.second.errorCode != nx::vms::common::update::Status::ErrorCode::noError);
                    // Fix for the cases when server does not report error code properly.
                    if (item->errorCode == nx::vms::common::update::Status::ErrorCode::noError)
                        changed |= compareAndSet(nx::vms::common::update::Status::ErrorCode::unknownError, item->errorCode);
                    changed |= compareAndSet(errorString(status.second.errorCode), item->statusMessage);

                    if (item->errorCode == common::update::Status::ErrorCode::installationError)
                    {
                        NX_INFO(this, "Installation failed for %1", item->id);
                        item->installing = false;
                        changed = true;
                    }
                }

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
                    changed = true;
                }

                if (changed)
                    itemsChanged.append(item);
            }
            else
            {
                NX_ERROR(this, "setUpdateStatus() got unknown peer id=%1", status.first);
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
                && (!item->offline || item->state != StatusCode::offline)
                && !item->incompatible)
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
        NX_MUTEX_LOCKER locker(&m_dataLock);
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
        NX_MUTEX_LOCKER locker(&m_dataLock);
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

void PeerStateTracker::resetOfflinePackagesInformation(const QSet<QnUuid>& targets)
{
    QList<UpdateItemPtr> itemsChanged;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
        for (const auto& id: targets)
        {
            if (auto item = findItemById(id))
            {
                item->offlinePackageProgress = 0;
                item->offlinePackagesStatus =
                    common::update::PackageDownloadStatus::Status::downloading;
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
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (auto& item: m_items)
    {
        // Offline peers are already 'clean' enough, ignoring them.
        if (item->state == StatusCode::offline && item->offline)
            continue;
        item->state = StatusCode::idle;
        item->progress = 0;
        item->statusMessage = tr("Waiting for peer data");
        item->verificationMessage = "";
        item->installing = false;
    }

    auto items = m_items;
    locker.unlock();

    for (auto item: m_items)
        emit itemChanged(item);
}

std::map<QnUuid, nx::vms::common::update::Status::Code> PeerStateTracker::allPeerStates() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    std::map<QnUuid, nx::vms::common::update::Status::Code> result;

    for (const auto& item: m_items)
        result.emplace(item->id, item->state);
    return result;
}

std::map<QnUuid, QnMediaServerResourcePtr> PeerStateTracker::activeServers() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);

    std::map<QnUuid, QnMediaServerResourcePtr> result;
    auto filter = m_peersIssued;
    for (const auto& item: m_items)
    {
        if (!filter.empty() && !filter.contains(item->id))
            continue;
        if (item->offline)
            continue;
        if (item->component != UpdateItem::Component::server)
            continue;
        if (auto server = getServer(item))
            result[item->id] = server;
    }
    return result;
}

QList<UpdateItemPtr> PeerStateTracker::allItems() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    return m_items;
}

QSet<QnUuid> PeerStateTracker::allPeers() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::peersInState(StatusCode state, bool withClient) const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!withClient && item->component == UpdateItem::Component::client)
            continue;
        if (item->state == state)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::offlineServers() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->offline)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::onlineAndInState(StatusCode state) const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (!item->offline && item->state == state)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::offlineAndInState(StatusCode state) const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
    {
        if (item->offline && item->state == state)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::legacyServers() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
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
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installing && !item->installed)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::peersCompleteInstall() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    QSet<QnUuid> result;
    for (const auto& item: m_items)
        if (item->installed || item->state == StatusCode::latestUpdateInstalled)
            result.insert(item->id);
    return result;
}

QSet<QnUuid> PeerStateTracker::serversWithChangedProtocol() const
{
    int protocol = nx::vms::api::protocolVersion();
    NX_MUTEX_LOCKER locker(&m_dataLock);
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
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (item->statusUnknown && !item->offline)
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::peersWithDownloaderError() const
{
    QSet<UpdateItem::ErrorCode> errorTypes =
    {
        UpdateItem::ErrorCode::downloadFailed,
        UpdateItem::ErrorCode::internalDownloaderError,
        UpdateItem::ErrorCode::noFreeSpaceToDownload,
        UpdateItem::ErrorCode::noFreeSpaceToExtract,
        UpdateItem::ErrorCode::noFreeSpaceToInstall,
    };
    QSet<QnUuid> result;
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (item->state != StatusCode::error)
            continue;
        if (errorTypes.contains(item->errorCode))
            result.insert(item->id);
    }
    return result;
}

QSet<QnUuid> PeerStateTracker::peersWithOfflinePackages() const
{
    QSet<QnUuid> result;
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (!item->offline && !item->isClient() && item->offlinePackageCount > 0)
            result.insert(item->id);
    }

    return result;
}

QSet<QnUuid> PeerStateTracker::peersCompletedOfflinePackagesDownload() const
{
    QSet<QnUuid> result;
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (!item->offline && !item->isClient()
            && item->offlinePackageProgress == item->offlinePackageCount * 100)
        {
            result.insert(item->id);
        }
    }

    return result;
}

QSet<QnUuid> PeerStateTracker::peersWithOfflinePackageDownloadError() const
{
    QSet<QnUuid> result;
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        if (!item->offline && !item->isClient()
            && (item->offlinePackagesStatus
                == common::update::PackageDownloadStatus::Status::corrupted))
        {
            result.insert(item->id);
        }
    }

    return result;
}

void PeerStateTracker::processUnknownStates()
{
    QList<UpdateItemPtr> itemsChanged;
    auto now = UpdateItem::Clock::now();

    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
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
                if (state != StatusCode::offline)
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
                else
                {
                    // Sometimes mediaserver resource has offline status, but this server is actually
                    // online and has state != StatusCode::offline at /ec2/updateStatus. So current
                    // logic causes constant switches to 'full offline' and back.
                    // Resetting offline timestamp to prevent such blinking.
                    item->lastOnlineTime = now;
                }
            }
        }
    }

    for (auto item: itemsChanged)
        emit itemChanged(item);
}

void PeerStateTracker::processDownloadTaskSet()
{
    NX_MUTEX_LOCKER locker(&m_dataLock);

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
                case StatusCode::starting:
                    // Most likely mediaserver did not have enough time to start downloading.
                    // Nothing to do here.
                    break;
            }
        }
    }
}

void PeerStateTracker::processReadyInstallTaskSet()
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& item: m_items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (item->incompatible)
            continue;

        if (m_peersFailed.contains(id) && state != StatusCode::error)
            m_peersFailed.remove(id);

        if (item->offline && state != StatusCode::offline)
        {
            if (m_peersActive.contains(id))
            {
                NX_VERBOSE(this,
                    "processReadyInstallTaskSet() - peer %1 just went offline from readyInstall. "
                    "I will wait a bit for its return.", id);
                m_peersActive.remove(id);
            }
        }
        else
        {
            switch (state)
            {
                case StatusCode::readyToInstall:
                    if (!m_peersIssued.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 is ready to install, "
                            "but was not tracked. Adding it to task set.", id);
                        m_peersActive.insert(id);
                        m_peersIssued.insert(id);
                    }
                    if (m_peersIssued.contains(id) && !m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 ready to start installation", id);
                        m_peersActive.insert(id);
                    }
                    // Nothing to do here.
                    break;
                case StatusCode::preparing:
                case StatusCode::downloading:
                case StatusCode::idle:
                case StatusCode::starting:
                    if (m_peersActive.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 has changed state to %2 and "
                            "no longer is ready to start installation", id, toString(state));
                        m_peersActive.remove(id);
                    }
                    break;
                case StatusCode::offline:
                    if (item->offline && m_peersIssued.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 is completely offline. "
                            "I will not wait for it.", id);
                        m_peersIssued.remove(id);
                        m_peersActive.remove(id);
                    }
                    // NOTE: When this server returns online, its 'idle' state will be ignored.
                    // Though when it goes to 'preparing' or 'downloading', client will switch
                    // to 'downloading' state as well, so this server will be properly tracked.
                    break;
                case StatusCode::error:
                    if (m_peersIssued.contains(id))
                    {
                        m_peersActive.remove(id);
                        m_peersFailed.insert(id);
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 has an error \"%2\".",
                            id, item->statusMessage);
                    }
                    break;
                case StatusCode::latestUpdateInstalled:
                    if (m_peersIssued.contains(id))
                    {
                        NX_VERBOSE(this,
                            "processReadyInstallTaskSet() - peer %1 has completed an update somehow.",
                            id);
                        m_peersActive.remove(id);
                        m_peersIssued.remove(id);
                    }
                    break;
            }
        }
    }
}

void PeerStateTracker::processInstallTaskSet()
{
    NX_MUTEX_LOCKER locker(&m_dataLock);

    for (const auto& item: m_items)
    {
        StatusCode state = item->state;
        auto id = item->id;

        if (item->incompatible)
            continue;

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
                case StatusCode::starting:
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
    NX_MUTEX_LOCKER locker(&m_dataLock);
    return m_peersActive;
}

QSet<QnUuid> PeerStateTracker::peersIssued() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    return m_peersIssued;
}

QSet<QnUuid> PeerStateTracker::peersFailed() const
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    return m_peersFailed;
}

void PeerStateTracker::setTask(const QSet<QnUuid> &targets, bool reset)
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    m_peersActive = targets;
    m_peersIssued = targets;

    if (reset)
    {
        m_peersComplete = {};
        m_peersFailed = {};
    }
}

void PeerStateTracker::setTaskError(const QSet<QnUuid>& targets, const QString& /*error*/)
{
    NX_MUTEX_LOCKER locker(&m_dataLock);
    for (const auto& id: targets)
    {
        if (auto item = findItemById(id))
            m_peersFailed.insert(id);
    }
}

void PeerStateTracker::addToTask(QnUuid id)
{
    m_peersIssued.insert(id);
    m_peersActive.insert(id);
}

void PeerStateTracker::removeFromTask(QnUuid id)
{
    m_peersIssued.remove(id);
    m_peersActive.remove(id);
    m_peersComplete.remove(id);
    m_peersFailed.remove(id);
}

QString PeerStateTracker::errorString(nx::vms::common::update::Status::ErrorCode code)
{
    using Code = nx::vms::common::update::Status::ErrorCode;

    switch (code)
    {
        case Code::noError:
            return "No error. It is a bug if you see this message.";
        case Code::updatePackageNotFound:
            return tr("Update package is not found.");
        case Code::osVersionNotSupported:
            return tr("This OS version is no longer supported.");
        case Code::noFreeSpaceToDownload:
            return tr("There is not enough space to download update files.");
        case Code::noFreeSpaceToExtract:
            return tr("There is not enough space to extract update files.");
        case Code::noFreeSpaceToInstall:
            return tr("There is not enough space to install update.");
        case Code::downloadFailed:
            return tr("Failed to download update packages.");
        case Code::invalidUpdateContents:
            return tr("Update contents are invalid.");
        case Code::corruptedArchive:
            return tr("Update archive is corrupted.");
        case Code::extractionError:
            return tr("Update files cannot be extracted.");
        case Code::verificationError:
            return tr("Update file verification failed.");
        case Code::internalDownloaderError:
            return tr("Internal downloader error.");
        case Code::internalError:
            return tr("Internal server error.");
        case Code::installationError:
            return tr("Update installation failed.");
        case Code::unknownError:
            return tr("Unknown error.");
    }
    NX_ASSERT(false);
    return tr("Unexpected error code.");
}

void PeerStateTracker::atResourceAdded(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || findItemById(server->getId()))
        return;

    if (m_serverFilter && !m_serverFilter(server))
        return;

    if (const auto status = server->getStatus();
        status == nx::vms::api::ResourceStatus::incompatible
        || status == nx::vms::api::ResourceStatus::unauthorized
        || status == nx::vms::api::ResourceStatus::mismatchedCertificate)
    {
        NX_VERBOSE(this, "%1(%2) - %3", __func__, server->getName(), status);
        return;
    }

    UpdateItemPtr item;
    {
        NX_MUTEX_LOCKER locker(&m_dataLock);
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
        NX_MUTEX_LOCKER locker(&m_dataLock);
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
        NX_MUTEX_LOCKER locker(&m_dataLock);
        item = findItemById(resource->getId());
        if (!item)
            return;
        changed = updateServerData(server, item);
    }
    if (changed)
        emit itemChanged(item);
}

void PeerStateTracker::atClientUpdateStateChanged(
    ClientUpdateTool::State state, int percentComplete, const ClientUpdateTool::Error& error)
{
    if (!m_clientItem)
    {
        NX_VERBOSE(this, "atClientupdateStateChanged(%1, %2) - client item is already destroyed",
            ClientUpdateTool::toString(state), percentComplete);
        return;
    }

    NX_VERBOSE(this, "atClientupdateStateChanged(%1, %2)",
        ClientUpdateTool::toString(state), percentComplete);

    m_clientItem->installing = false;
    m_clientItem->installed = false;
    m_clientItem->progress = 0;

    switch (state)
    {
        case ClientUpdateTool::State::initial:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = tr("No update task");
            break;
        case ClientUpdateTool::State::readyDownload:
            m_clientItem->state = StatusCode::idle;
            m_clientItem->statusMessage = tr("Ready to download update");
            break;
        case ClientUpdateTool::State::downloading:
            m_clientItem->state = StatusCode::downloading;
            m_clientItem->progress = percentComplete;
            m_clientItem->statusMessage = tr("Downloading update");
            break;
        case ClientUpdateTool::State::verifying:
            m_clientItem->state = StatusCode::preparing;
            m_clientItem->progress = percentComplete;
            m_clientItem->statusMessage = tr("Verifying update");
            break;
        case ClientUpdateTool::State::readyInstall:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = tr("Ready to install update");
            break;
        case ClientUpdateTool::State::readyRestart:
            m_clientItem->statusMessage = tr("Ready to restart to the new version");
            m_clientItem->state = StatusCode::readyToInstall;
            NX_ASSERT(!m_targetVersion.isNull());
            m_clientItem->version = m_targetVersion;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            break;
        case ClientUpdateTool::State::installing:
            m_clientItem->state = StatusCode::readyToInstall;
            m_clientItem->installing = true;
            m_clientItem->statusMessage = tr("Installing update");
            break;
        case ClientUpdateTool::State::complete:
            m_clientItem->state = StatusCode::latestUpdateInstalled;
            NX_ASSERT(!m_targetVersion.isNull());
            m_clientItem->version = m_targetVersion;
            m_clientItem->installing = false;
            m_clientItem->installed = true;
            m_clientItem->progress = 100;
            m_clientItem->statusMessage = tr("Installed");
            break;
        case ClientUpdateTool::State::error:
            m_clientItem->state = StatusCode::error;
            m_clientItem->statusMessage = ClientUpdateTool::errorString(error);
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
    connect(server.data(), &QnMediaServerResource::serverFlagsChanged,
        this, &PeerStateTracker::atResourceChanged);

    if (auto serverResource = server.dynamicCast<ServerResource>(); NX_ASSERT(serverResource))
    {
        connect(serverResource.data(), &ServerResource::compatibilityChanged,
            this, &PeerStateTracker::atResourceChanged);
    }
    return item;
}

UpdateItemPtr PeerStateTracker::addItemForClient(nx::vms::common::SystemContext* context)
{
    UpdateItemPtr item = std::make_shared<UpdateItem>();

    item->id = getClientPeerId(context);
    item->component = UpdateItem::Component::client;
    item->row = m_items.size();
    item->protocol = nx::vms::api::protocolVersion();
    m_clientItem = item;
    m_items.push_back(item);
    updateClientData();

    NX_VERBOSE(this, "addItemForClient() id=%1 version=%2", item->id, item->version);
    emit itemAdded(m_clientItem);
    return m_clientItem;
}

bool PeerStateTracker::updateServerData(QnMediaServerResourcePtr server, UpdateItemPtr item)
{
    // NOTE: We should be wrapped by a mutex here.
    bool changed = false;
    auto status = server->getStatus();

    bool incompatible = status == nx::vms::api::ResourceStatus::incompatible;

    if (const auto serverResource = server.dynamicCast<ServerResource>(); NX_ASSERT(serverResource))
        incompatible |= !serverResource->isCompatible();

    if (incompatible != item->onlyLegacyUpdate)
    {
        item->onlyLegacyUpdate = incompatible;
        changed = true;
    }

    if (incompatible != item->incompatible)
    {
        NX_VERBOSE(this, "updateServerData() - peer %1 changing incompatible=%2",
            item->id, incompatible);
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
            NX_VERBOSE(this, "updateServerData() - peer %1 goes offline", item->id);
        }
        else
        {
            if (item->state == StatusCode::readyToInstall)
            {
                NX_VERBOSE(this,
                    "updateServerData() - peer %1 gone offline from state readyInstall.",
                    item->id);
                item->state = StatusCode::preparing;
            }
            // We need to get /ec2/updateStatus to get a proper status.
            item->statusUnknown = true;
            // TODO: Should track offline->online changes for task sets:
            //  - peersActive
            //  - peersFailed
        }
        item->offline = viewAsOffline;
        // TODO: Should take this away, out of the mutex scope.
        emit itemOnlineStatusChanged(item);
        changed = true;
    }

    bool hasInternet = server->hasInternetAccess();
    if (hasInternet != item->hasInternet)
    {
        NX_INFO(this, "updateServerData() - peer %1 changing hasInternet to %2", item->id, hasInternet);
        item->hasInternet = hasInternet;
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
        if (installed && item->installing)
        {
            // According to VMS-15379 we can wait for quite long time until server will respond
            // with a proper /ec2/updateStatus during install stage. Servers will hang in
            // 'statusUnknown' state for this period. Here we try to clean up 'statusUnknown'
            // flag before current server goes online and responds to /ec2/updateStatus.
            item->installing = false;
            item->statusUnknown = false;
            item->lastStatusTime = UpdateItem::Clock::now();
        }
        item->installed = installed;
        NX_INFO(this, "updateServerData() - peer %1 changed installed=%2", item->id, item->installed);

        changed = true;
    }

    auto moduleInfo = server->getModuleInformation();
    if (moduleInfo.protoVersion != item->protocol)
    {
        NX_INFO(this, "updateServerData() - peer %1 has changed protocol version %2 to %3",
            item->id, item->protocol, moduleInfo.protoVersion);
        item->protocol = moduleInfo.protoVersion;
        changed = true;
    }

    return changed;
}

bool PeerStateTracker::updateClientData()
{
    bool changed = (m_clientItem->version != appContext()->version());
    m_clientItem->version = appContext()->version();
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
