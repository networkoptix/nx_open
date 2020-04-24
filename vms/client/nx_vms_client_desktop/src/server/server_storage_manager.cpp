#include "server_storage_manager.h"

#include <chrono>

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/storage_space_reply.h>
#include <common/common_module.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/client_storage_resource.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>

#include <nx/utils/range_adapters.h>

namespace {

using namespace std::chrono;

// Delay between requests when the rebuild is running.
static constexpr auto kUpdateRebuildStatusDelay = 1s;

// Delay between requests when the backup is running.
static constexpr auto kUpdateBackupStatusDelay = 1s;

void processStorage(const QnClientStorageResourcePtr& storage, const QnStorageSpaceData& spaceInfo)
{
    storage->setFreeSpace(spaceInfo.freeSpace);
    storage->setTotalSpace(spaceInfo.totalSpace);
    storage->setWritable(spaceInfo.isWritable);
    storage->setSpaceLimit(spaceInfo.reservedSpace);
    storage->setStatus(spaceInfo.isOnline ? Qn::Online : Qn::Offline);
    storage->setStatusFlag(spaceInfo.storageStatus);
}

QnMediaServerResourcePtr getServerForResource(const QnResourcePtr& resource)
{
    if (const auto server = resource.objectCast<QnMediaServerResource>())
        return server;

    if (const auto storage = resource.objectCast<QnStorageResource>())
        return storage->getParentServer();

    return {};
};

bool isActive(const QnStorageResourcePtr& storage)
{
    return storage
        && !storage->flags().testFlag(Qn::removed)
        && storage->isWritable() && storage->isUsedForWriting()
        && storage->isOnline();
}

} // namespace

struct ServerPoolInfo
{
    QnStorageScanData rebuildStatus;
};

struct QnServerStorageManager::ServerInfo
{
    std::array<ServerPoolInfo, static_cast<int>(QnServerStoragesPool::Count)> storages{};
    QSet<QString> protocols;
    bool fastInfoReady = false;
    QnBackupStatusData backupStatus;
};

QnServerStorageManager::RequestKey::RequestKey(
    const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool)
    :
    server(server),
    pool(pool)
{
}

QnServerStorageManager::QnServerStorageManager(QObject* parent):
    base_type(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnServerStorageManager::handleResourceAdded);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnServerStorageManager::handleResourceRemoved);

    const auto allServers = resourcePool()->getAllServers(Qn::AnyStatus);

    for (const auto& server: allServers)
        handleResourceAdded(server);
}

QnServerStorageManager::~QnServerStorageManager()
{
}

void QnServerStorageManager::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto emitStorageChanged =
        [this](const QnResourcePtr& resource)
        {
            const auto storage = resource.objectCast<QnStorageResource>();
            if (NX_ASSERT(storage))
                emit storageChanged(storage);
        };

    const auto handleStorageChanged =
        [this](const QnResourcePtr& resource)
        {
            const auto storage = resource.objectCast<QnStorageResource>();
            if (!NX_ASSERT(storage))
                return;

            emit storageChanged(resource.objectCast<QnStorageResource>());

            const auto server = storage->getParentServer();
            if (server && server->metadataStorageId() == resource->getId())
                updateActiveMetadataStorage(server);
        };

    if (const auto& storage = resource.objectCast<QnClientStorageResource>())
    {
        connect(storage, &QnResource::statusChanged,
            this, &QnServerStorageManager::checkStoragesStatusInternal);

        connect(storage, &QnResource::statusChanged, this, handleStorageChanged);

        // Free space is not used in the client but called quite often.
        //connect(storage, &QnClientStorageResource::freeSpaceChanged,
        //    this, emitStorageChanged);

        connect(storage, &QnClientStorageResource::totalSpaceChanged, this, emitStorageChanged);
        connect(storage, &QnClientStorageResource::isWritableChanged, this, handleStorageChanged);

        connect(storage, &QnClientStorageResource::isUsedForWritingChanged,
            this, handleStorageChanged);

        connect(storage, &QnClientStorageResource::isBackupChanged, this, emitStorageChanged);
        connect(storage, &QnClientStorageResource::isActiveChanged, this, emitStorageChanged);

        emit storageAdded(storage);
        checkStoragesStatusInternal(storage);
        return;
    }

    const auto server = resource.objectCast<QnMediaServerResource>();
    if (!server || server.objectCast<QnFakeMediaServerResource>())
        return;

    m_serverInfo.insert(server, ServerInfo());

    connect(server, &QnMediaServerResource::statusChanged,
        this, &QnServerStorageManager::checkStoragesStatusInternal);

    connect(server, &QnMediaServerResource::apiUrlChanged,
        this, &QnServerStorageManager::checkStoragesStatusInternal);

    connect(server, &QnMediaServerResource::propertyChanged, this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key == QnMediaServerResource::kMetadataStorageIdKey)
                updateActiveMetadataStorage(resource.objectCast<QnMediaServerResource>());
        });

    checkStoragesStatus(server);
};

void QnServerStorageManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto& storage = resource.objectCast<QnStorageResource>())
    {
        storage->disconnect(this);
        checkStoragesStatusInternal(storage);
        emit storageRemoved(storage);

        const auto server = storage->getParentServer();
        if (server && activeMetadataStorage(server) == storage)
            updateActiveMetadataStorage(server);

        return;
    }

    const auto server = resource.objectCast<QnMediaServerResource>();
    if (!server || server.objectCast<QnFakeMediaServerResource>())
        return;

    m_serverInfo.remove(server);
    server->disconnect(this);

    updateActiveMetadataStorage(server);
};

void QnServerStorageManager::invalidateRequests()
{
    m_requests.clear();
}

bool QnServerStorageManager::isServerValid(const QnMediaServerResourcePtr& server) const
{
    // Server is invalid.
    if (!server || !server->apiConnection())
        return false;

    // Server is not watched.
    if (!m_serverInfo.contains(server))
        return false;

    return server->getStatus() == Qn::Online;
}

QSet<QString> QnServerStorageManager::protocols(const QnMediaServerResourcePtr& server) const
{
    if (!m_serverInfo.contains(server))
        return QSet<QString>();

    return m_serverInfo[server].protocols;
}

QnStorageScanData QnServerStorageManager::rebuildStatus(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool) const
{
    if (!m_serverInfo.contains(server))
        return QnStorageScanData();

    return m_serverInfo[server].storages[static_cast<int>(pool)].rebuildStatus;
}

QnBackupStatusData QnServerStorageManager::backupStatus(
    const QnMediaServerResourcePtr& server) const
{
    if (!m_serverInfo.contains(server))
        return QnBackupStatusData();

    return m_serverInfo[server].backupStatus;
}

bool QnServerStorageManager::rebuildServerStorages(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
{
    NX_VERBOSE(this, "Starting rebuild %1 of %2", (int)pool, server);
    if (!sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Start))
        return false;

    auto& serverInfo = m_serverInfo[server];
    auto& poolInfo = serverInfo.storages[static_cast<int>(pool)];
    poolInfo.rebuildStatus.state = Qn::RebuildState_FullScan;
    NX_VERBOSE(this, "Initializing status of pool %1 to fullscan", (int)pool);

    emit serverRebuildStatusChanged(server, pool, poolInfo.rebuildStatus);

    return true;
}

bool QnServerStorageManager::cancelServerStoragesRebuild(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
{
    return sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Cancel);
}

bool QnServerStorageManager::backupServerStorages(const QnMediaServerResourcePtr& server)
{
    return sendBackupRequest(server, Qn::BackupAction_Start);
}

bool QnServerStorageManager::cancelBackupServerStorages(const QnMediaServerResourcePtr& server)
{
    return sendBackupRequest(server, Qn::BackupAction_Cancel);
}

void QnServerStorageManager::checkStoragesStatus(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return;

    updateActiveMetadataStorage(server);

    if (!isServerValid(server))
        return;

    NX_VERBOSE(this, "Check storages status on server changes");
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Main);
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Backup);
    sendBackupRequest(server);
}

void QnServerStorageManager::checkStoragesStatusInternal(const QnResourcePtr& resource)
{
    checkStoragesStatus(getServerForResource(resource));
}

// TODO: #GDM SafeMode
void QnServerStorageManager::saveStorages(const QnStorageResourceList& storages)
{
    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(storages, apiStorages);

    connection->getMediaServerManager(Qn::kSystemAccess)->saveStorages(apiStorages, this,
        [storages](int /*reqID*/, ec2::ErrorCode error)
        {
            if (error != ec2::ErrorCode::ok)
                return;

            for (const auto& storage: storages)
            {
                if (const auto& clientStorage = storage.objectCast<QnClientStorageResource>())
                    clientStorage->setActive(true);
            }
        });

    invalidateRequests();
}

// TODO: #GDM SafeMode
void QnServerStorageManager::deleteStorages(const nx::vms::api::IdDataList& ids)
{
    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    connection->getMediaServerManager(Qn::kSystemAccess)->removeStorages(ids, this, []() {});
    invalidateRequests();
}

bool QnServerStorageManager::sendArchiveRebuildRequest(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool, Qn::RebuildAction action)
{
    if (!isServerValid(server))
        return false;

    const int handle = server->apiConnection()->doRebuildArchiveAsync(
        action,
        pool == QnServerStoragesPool::Main,
        this,
        SLOT(at_archiveRebuildReply(int, const QnStorageScanData&, int)));

    NX_VERBOSE(this, "Send request %1 to rebuild pool %2 of %3 (action %4)",
        handle, (int)pool, server, action);
    if (handle <= 0)
        return false;

    if (action != Qn::RebuildAction_ShowProgress)
        invalidateRequests();

    m_requests.insert(handle, RequestKey(server, pool));
    return true;
}

void QnServerStorageManager::at_archiveRebuildReply(
    int status, const QnStorageScanData& reply, int handle)
{
    NX_VERBOSE(this, "Received archive rebuild reply %1, status %2, %3", handle, status, reply);
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);
    NX_VERBOSE(this, "Target server %1, pool %2", requestKey.server, (int)requestKey.pool);

    // Whether server was removed from the pool.
    if (!m_serverInfo.contains(requestKey.server))
        return;

    auto& serverInfo = m_serverInfo[requestKey.server];
    auto& poolInfo = serverInfo.storages[static_cast<int>(requestKey.pool)];

    Callback delayedCallback;
    if ((reply.state > Qn::RebuildState_None) || (status != 0))
    {
        NX_VERBOSE(this, "Queue next rebuild progress request");
        delayedCallback =
            [this, requestKey]()
            {
                NX_VERBOSE(this, "Sending delayed info request");
                sendArchiveRebuildRequest(requestKey.server, requestKey.pool);
            };
    }
    else
    {
        NX_VERBOSE(this, "Queue space request");
        delayedCallback =
            [this, requestKey]()
            {
                sendStorageSpaceRequest(requestKey.server);
            };
    }

    executeDelayedParented(delayedCallback, kUpdateRebuildStatusDelay, this);

    if (status != 0)
        return;

    if (poolInfo.rebuildStatus != reply)
    {
        const bool finished = (poolInfo.rebuildStatus.state == Qn::RebuildState_FullScan
            && reply.state == Qn::RebuildState_None);

        NX_VERBOSE(this, "Rebuild status changed, is finished: %1", finished);

        poolInfo.rebuildStatus = reply;
        emit serverRebuildStatusChanged(requestKey.server, requestKey.pool, reply);

        if (finished)
            emit serverRebuildArchiveFinished(requestKey.server, requestKey.pool);
    }

}

bool QnServerStorageManager::sendBackupRequest(
    const QnMediaServerResourcePtr& server, Qn::BackupAction action)
{
    if (!isServerValid(server))
        return false;

    const int handle = server->apiConnection()->backupControlActionAsync(
        action,
        this,
        SLOT(at_backupStatusReply(int, const QnBackupStatusData&, int)));

    if (handle <= 0)
        return false;

    if (action != Qn::BackupAction_ShowProgress)
        invalidateRequests();

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    return true;
}

void QnServerStorageManager::at_backupStatusReply(
    int status, const QnBackupStatusData& reply, int handle)
{
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);

    // Server was removed from the pool.
    if (!m_serverInfo.contains(requestKey.server))
        return;

    auto& serverInfo = m_serverInfo[requestKey.server];
    if (reply.state == Qn::BackupState_InProgress || status != 0)
    {
        const auto delayedCallback = [this, requestKey]() { sendBackupRequest(requestKey.server); };
        executeDelayedParented(delayedCallback, kUpdateBackupStatusDelay, this);
    }

    if (status != 0)
        return;

    if (serverInfo.backupStatus != reply)
    {
        const bool finished = (serverInfo.backupStatus.state == Qn::BackupState_InProgress
            && reply.state == Qn::BackupState_None);

        serverInfo.backupStatus = reply;
        emit serverBackupStatusChanged(requestKey.server, reply);

        if (finished)
            emit serverBackupFinished(requestKey.server);
    }
}

bool QnServerStorageManager::sendStorageSpaceRequest(const QnMediaServerResourcePtr& server)
{
    if (!isServerValid(server))
        return false;

    const bool sendFastRequest = !m_serverInfo[server].fastInfoReady;

    const int handle = server->apiConnection()->getStorageSpaceAsync(
        sendFastRequest,
        this,
        SLOT(at_storageSpaceReply(int, const QnStorageSpaceReply&, int)));

    if (handle <= 0)
        return false;

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    return true;
}

void QnServerStorageManager::at_storageSpaceReply(
    int status, const QnStorageSpaceReply& reply, int handle)
{
    for (const auto& spaceInfo: reply.storages)
    {
        if (const auto storage = resourcePool()->getResourceById<QnClientStorageResource>(
            spaceInfo.storageId))
        {
            processStorage(storage, spaceInfo);
        }
    }

    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);

    if (status != 0)
        return;

    // Server was removed from the pool.
    if (!m_serverInfo.contains(requestKey.server))
        return;

    auto& serverInfo = m_serverInfo[requestKey.server];
    if (!serverInfo.fastInfoReady)
    {
        // Immediately send usual request if we have received the fast one.
        serverInfo.fastInfoReady = true;
        sendStorageSpaceRequest(requestKey.server);
    }

    // Reply can contain some storages that were instantiated after the server starts.
    // Therefore they will be absent in the resource pool, but user can add them manually.
    // Also we should manually remove these storages from pool if they were not added on the
    // server side and removed before getting into database.
    // This code should be executed if and only if server still exists and our request is actual.
    QSet<QString> existingStorageUrls;
    for (const auto& storage: requestKey.server->getStorages())
        existingStorageUrls.insert(storage->getUrl());

    for (const auto& spaceInfo: reply.storages)
    {
        // Skip storages that are already exist.
        if (existingStorageUrls.contains(spaceInfo.url))
            continue;

        // If we've just deleted a storage, we can receive its space info from the previous request.
        // Just skip it.
        if (!spaceInfo.storageId.isNull())
            continue;

        const auto storage = QnClientStorageResource::newStorage(requestKey.server, spaceInfo.url);
        storage->setStorageType(spaceInfo.storageType);

        resourcePool()->addResource(storage);
        processStorage(storage, spaceInfo);

        existingStorageUrls.insert(spaceInfo.url);
    }

    QSet<QString> receivedStorageUrls;
    for (const auto& spaceInfo: reply.storages)
        receivedStorageUrls.insert(spaceInfo.url);

    QnResourceList storagesToDelete;
    for (const auto& storage: requestKey.server->getStorages())
    {
        // Skipping storages that are really present in the server resource pool.
        // They will be deleted on storageRemoved transaction.
        const auto clientStorage = storage.objectCast<QnClientStorageResource>();
        NX_ASSERT(clientStorage, "Only client storage intances must exist on the client side.");
        if (clientStorage && clientStorage->isActive())
            continue;

        // Skipping storages that are confirmed by server.
        if (receivedStorageUrls.contains(storage->getUrl()))
            continue;

        // Removing other storages (e.g. external drives that were switched off.
        storagesToDelete.push_back(storage);
    }

    resourcePool()->removeResources(storagesToDelete);

    const auto replyProtocols = reply.storageProtocols.toSet();
    if (serverInfo.protocols != replyProtocols)
    {
        serverInfo.protocols = replyProtocols;
        emit serverProtocolsChanged(requestKey.server, replyProtocols);
    }
}

QnStorageResourcePtr QnServerStorageManager::activeMetadataStorage(
    const QnMediaServerResourcePtr& server) const
{
    return m_activeMetadataStorages.value(server);
}

void QnServerStorageManager::setActiveMetadataStorage(const QnMediaServerResourcePtr& server,
    const QnStorageResourcePtr& storage)
{
    if (!NX_ASSERT(server))
        return;

    if (m_activeMetadataStorages.value(server) == storage)
        return;

    m_activeMetadataStorages[server] = storage;
    emit activeMetadataStorageChanged(server);
}

void QnServerStorageManager::updateActiveMetadataStorage(const QnMediaServerResourcePtr& server)
{
    setActiveMetadataStorage(server, calculateActiveMetadataStorage(server));
}

QnStorageResourcePtr QnServerStorageManager::calculateActiveMetadataStorage(
    const QnMediaServerResourcePtr& server) const
{
    if (!isServerValid(server)) //< Includes online/offline check.
        return {};

    const auto storageId = server->metadataStorageId();
    if (storageId.isNull())
        return {};

    const auto storage = resourcePool()->getResourceById<QnStorageResource>(storageId);
    return isActive(storage) && storage->getParentServer() == server
        ? storage
        : QnStorageResourcePtr();
}
