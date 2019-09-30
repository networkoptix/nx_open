#include "server_storage_manager.h"

#include <api/app_server_connection.h>
#include <api/server_rest_connection.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/storage_space_reply.h>

#include <common/common_module.h>

#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/ec_api.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/client_storage_resource.h>

#include <utils/common/delayed.h>
#include <nx/utils/guarded_callback.h>
#include <core/resource/fake_media_server.h>

namespace {
    /** Delay between requests when the rebuild is running. */
    const int updateRebuildStatusDelayMs = 500;

    /** Delay between requests when the backup is running. */
    const int updateBackupStatusDelayMs = 500;

    void processStorage(
        const QnClientStorageResourcePtr& storage,
        const QnStorageSpaceData& spaceInfo)
    {
        storage->setFreeSpace(spaceInfo.freeSpace);
        storage->setTotalSpace(spaceInfo.totalSpace);
        storage->setWritable(spaceInfo.isWritable);
        storage->setSpaceLimit(spaceInfo.reservedSpace);
        storage->setStatus(spaceInfo.isOnline ? Qn::Online : Qn::Offline);
        storage->setStatusFlag(spaceInfo.storageStatus);
    }

    /**
     * Helper method to generate a callback, compatible with rest::ServerConenction::postJsonResult
     * It attaches to an object with type `Class` and a method with a signature:
     * `void someMethod(bool success, int handle, const TargetType& data)`
     */
    template<class Class, class TargetType> auto methodCallback(
        Class* object,
        void (Class::* method)(bool, int, const TargetType&))
    {
        NX_ASSERT(object);
        return nx::utils::guarded(object,
            [object, method](bool success, int handle, QnJsonRestResult jsonData) -> void
            {
                if (success)
                {
                    TargetType data = jsonData.deserialized<TargetType>(&success);
                    (object->*method)(success, handle, data);
                }
                else
                {
                    (object->*method)(success, handle, {});
                }
            });
    }

    template <class TargetType> auto simpleCallback(
        std::function<void (bool, int, const TargetType&)> callback)
    {
        return [callback](bool success, int handle, QnJsonRestResult jsonData) -> void
            {
                if (success)
                {
                    TargetType data = jsonData.deserialized<TargetType>(&success);
                    callback(success, handle, data);
                }
                else
                {
                    callback(success, handle, {});
                }
            };
    }
}

struct ServerPoolInfo
{
    QnStorageScanData rebuildStatus;

    ServerPoolInfo()
        : rebuildStatus()
    {}
};

struct QnServerStorageManager::ServerInfo
{
    std::array<ServerPoolInfo, static_cast<int>(QnServerStoragesPool::Count)> storages;
    QSet<QString> protocols;
    bool fastInfoReady;
    QnBackupStatusData backupStatus;

    ServerInfo()
        : storages()
        , protocols()
        , fastInfoReady(false)
        , backupStatus()
    {}
};

QnServerStorageManager::RequestKey::RequestKey()
    : server(QnMediaServerResourcePtr())
    , pool(QnServerStoragesPool::Main)
{}

QnServerStorageManager::RequestKey::RequestKey(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
    : server(server)
    , pool(pool)
{}

QnServerStorageManager::QnServerStorageManager(QObject *parent)
    : base_type(parent)
{
    auto getServerForResource =
        [](const QnResourcePtr &resource) -> QnMediaServerResourcePtr
        {
            if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
                return server;
            if (QnStorageResourcePtr storage = resource.dynamicCast<QnStorageResource>())
                return storage->getParentServer();
            return QnMediaServerResourcePtr();
        };

    auto checkStoragesStatusInternal =
        [this, getServerForResource](const QnResourcePtr &resource)
        {
            checkStoragesStatus(getServerForResource(resource));
        };

    auto resourceAdded = [this, checkStoragesStatusInternal](const QnResourcePtr &resource)
    {
        auto emitStorageChanged = [this](const QnResourcePtr &resource)
            {
                emit storageChanged(resource.dynamicCast<QnStorageResource>());
            };

        if (const QnClientStorageResourcePtr& storage = resource.dynamicCast<QnClientStorageResource>())
        {
            connect(storage, &QnResource::statusChanged, this, checkStoragesStatusInternal);
            // free space is not used in the client but called quite often
            //connect(storage, &QnClientStorageResource::freeSpaceChanged,    this, emitStorageChanged);
            connect(storage, &QnResource::statusChanged,                        this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::totalSpaceChanged,       this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::isWritableChanged,       this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::isUsedForWritingChanged, this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::isBackupChanged,         this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::isActiveChanged,         this, emitStorageChanged);

            emit storageAdded(storage);
            checkStoragesStatusInternal(storage);
            return;
        }

        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server || server.dynamicCast<QnFakeMediaServerResource>())
            return;

        m_serverInfo.insert(server, ServerInfo());

        connect(server,   &QnMediaServerResource::statusChanged,  this,   checkStoragesStatusInternal);
        connect(server,   &QnMediaServerResource::apiUrlChanged,  this,   checkStoragesStatusInternal);
        checkStoragesStatus(server);
    };

    auto resourceRemoved =
        [this, checkStoragesStatusInternal](const QnResourcePtr& resource)
        {
            if (const QnStorageResourcePtr& storage = resource.dynamicCast<QnStorageResource>())
            {
                disconnect(storage, nullptr, this, nullptr);
                checkStoragesStatusInternal(storage);
                emit storageRemoved(storage);
                return;
            }

            QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
            if (!server || server.dynamicCast<QnFakeMediaServerResource>())
                return;

            m_serverInfo.remove(server);
            disconnect(server, nullptr, this, nullptr);
        };

    connect(resourcePool(), &QnResourcePool::resourceAdded,      this, resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,    this, resourceRemoved);

    const auto allServers = resourcePool()->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr &server: allServers)
        resourceAdded(server);

}

QnServerStorageManager::~QnServerStorageManager()
{

}

void QnServerStorageManager::invalidateRequests()
{
    m_requests.clear();
}

bool QnServerStorageManager::isServerValid(const QnMediaServerResourcePtr& server) const
{
    /* Server is invalid. */
    if (!server || !server->restConnection())
        return false;

    /* Server is not watched. */
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
    if (!sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Start))
        return false;

    ServerInfo& serverInfo = m_serverInfo[server];
    ServerPoolInfo& poolInfo = serverInfo.storages[static_cast<int>(pool)];
    poolInfo.rebuildStatus.state = Qn::RebuildState_FullScan;
    emit serverRebuildStatusChanged(server, pool, poolInfo.rebuildStatus);

    return true;
}

bool QnServerStorageManager::cancelServerStoragesRebuild(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
{
    return sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Cancel);
}

bool QnServerStorageManager::backupServerStorages( const QnMediaServerResourcePtr& server )
{
    return sendBackupRequest(server, Qn::BackupAction_Start);
}

bool QnServerStorageManager::cancelBackupServerStorages( const QnMediaServerResourcePtr& server )
{
    return sendBackupRequest(server, Qn::BackupAction_Cancel);
}

void QnServerStorageManager::checkStoragesStatus( const QnMediaServerResourcePtr& server )
{
    if (!isServerValid(server))
        return;

    sendArchiveRebuildRequest(server, QnServerStoragesPool::Main);
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Backup);
    sendBackupRequest(server);
}

// TODO: #GDM SafeMode
void QnServerStorageManager::saveStorages(const QnStorageResourceList &storages )
{
    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    if (!conn)
        return;

    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(storages, apiStorages);

    conn->getMediaServerManager(Qn::kSystemAccess)->saveStorages(apiStorages, this,
        [storages](int reqID, ec2::ErrorCode error)
        {
            Q_UNUSED(reqID);
            if (error != ec2::ErrorCode::ok)
                return;

            for (const QnStorageResourcePtr& storage: storages)
                if (const QnClientStorageResourcePtr& clientStorage = storage.dynamicCast<QnClientStorageResource>())
                    clientStorage->setActive(true);
        });
    invalidateRequests();
}

// TODO: #GDM SafeMode
void QnServerStorageManager::deleteStorages(const nx::vms::api::IdDataList& ids )
{
    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    if (!conn)
        return;

    conn->getMediaServerManager(Qn::kSystemAccess)->removeStorages(ids, this, [] {});
    invalidateRequests();
}

bool QnServerStorageManager::sendArchiveRebuildRequest(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool, Qn::RebuildAction action)
{
    if (!isServerValid(server))
        return false;

    QnRequestParamList params;
    params.insert("action", QnLexical::serialized(action));
    params.insert("mainPool", pool == QnServerStoragesPool::Main);
    auto connection = server->restConnection();
    if (!connection)
        return false;

    auto handle = connection->getJsonResult(
        "/api/rebuildArchive", params,
        methodCallback(this, &QnServerStorageManager::at_archiveRebuildReply), thread());

    if (!handle)
        return false;

    if (action != Qn::RebuildAction_ShowProgress)
        invalidateRequests();

    m_requests.insert(handle, RequestKey(server, pool));
    return true;
}

void QnServerStorageManager::at_archiveRebuildReply(
    bool success, int handle, const QnStorageScanData& reply)
{
    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    if (!success)
        NX_ERROR(this, "at_archiveRebuildReply() - request failed");

    const auto requestKey = m_requests.take(handle);

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo& serverInfo = m_serverInfo[requestKey.server];
    ServerPoolInfo& poolInfo = serverInfo.storages[static_cast<int>(requestKey.pool)];

    Callback timerCallback;
    if ((reply.state > Qn::RebuildState_None) || !success)
    {
        timerCallback =
            [this, requestKey]() { sendArchiveRebuildRequest(requestKey.server, requestKey.pool); };
    }
    else
    {
        timerCallback =
            [this, requestKey]() { requestStorageSpace(requestKey.server); };
    }
    executeDelayedParented(timerCallback, updateRebuildStatusDelayMs, this);

    if(!success)
        return;

    if (poolInfo.rebuildStatus != reply)
    {
        bool finished = (poolInfo.rebuildStatus.state == Qn::RebuildState_FullScan
            && reply.state == Qn::RebuildState_None);

        poolInfo.rebuildStatus = reply;
        emit serverRebuildStatusChanged(requestKey.server, requestKey.pool, reply);

        if (finished)
            emit serverRebuildArchiveFinished(requestKey.server, requestKey.pool);
    }
}

bool QnServerStorageManager::sendBackupRequest(
    const QnMediaServerResourcePtr&server, Qn::BackupAction action)
{
    if (!isServerValid(server))
        return false;

    auto connection = server->restConnection();

    QnRequestParamList params;
    auto actionCode = QnLexical::serialized(action);
    if (!actionCode.isEmpty())
        params.insert("action", actionCode);

    auto handle = connection->getJsonResult("/api/backupControl", params,
        methodCallback(this, &QnServerStorageManager::at_backupStatusReply), thread());

    if (!handle)
        return false;

    if (action != Qn::BackupAction_ShowProgress)
        invalidateRequests();

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    return true;
}

void QnServerStorageManager::at_backupStatusReply(
    bool success, int handle, const QnBackupStatusData& reply)
{
    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    if (!success)
        NX_ERROR(this, "at_backupStatusReply() - request failed");

    const auto requestKey = m_requests.take(handle);

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo &serverInfo = m_serverInfo[requestKey.server];
    if (reply.state == Qn::BackupState_InProgress || !success)
    {
        const auto timerCallback = [this, requestKey]() { sendBackupRequest(requestKey.server); };
        executeDelayedParented(timerCallback, updateBackupStatusDelayMs, this);
    }

    if (!success)
        return;

    if (serverInfo.backupStatus != reply)
    {
        bool finished = (serverInfo.backupStatus.state == Qn::BackupState_InProgress
            && reply.state == Qn::BackupState_None);

        serverInfo.backupStatus = reply;
        emit serverBackupStatusChanged(requestKey.server, reply);

        if (finished)
            emit serverBackupFinished(requestKey.server);
    }
}

int QnServerStorageManager::requestStorageSpace(
    const QnMediaServerResourcePtr& server)
{
    if (!isServerValid(server))
        return 0;

    auto connection = server->restConnection();

    QnRequestParamList params;
    bool sendFastRequest = !m_serverInfo[server].fastInfoReady;
    if (sendFastRequest)
        params.insert("fast", QnLexical::serialized(true));

    int handle = connection->getJsonResult("/api/storageSpace", params,
        methodCallback(this, &QnServerStorageManager::at_storageSpaceReply), thread());

    if (!handle)
        return 0;

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    return handle;
}

int QnServerStorageManager::requestStorageStatus(
    const QnMediaServerResourcePtr& server, const QString& storageUrl,
    std::function<void (bool, int, const QnStorageStatusReply&)> callback)
{
    if (!isServerValid(server))
        return 0;

    QnRequestParamList params;
    params.insert("path", storageUrl);
    auto connection = server->restConnection();
    return connection->getJsonResult("/api/storageStatus", params,
        simpleCallback(callback), thread());
}

int QnServerStorageManager::requestRecordingStatistics(const QnMediaServerResourcePtr& server,
    qint64 bitrateAnalyzePeriodMs,
    std::function<void (bool, int, const QnRecordingStatsReply&)> callback)
{
    if (!isServerValid(server))
        return 0;
    QnRequestParamList params;
    params.insert("bitrateAnalyzePeriodMs", bitrateAnalyzePeriodMs);
    auto connection = server->restConnection();
    return connection->getJsonResult("/api/recStats", params,
        simpleCallback(callback), thread());
}

void QnServerStorageManager::at_storageSpaceReply(
    bool success, int handle, const QnStorageSpaceReply& reply)
{
    for (const QnStorageSpaceData& spaceInfo: reply.storages)
    {
        QnClientStorageResourcePtr storage =
            resourcePool()->getResourceById<QnClientStorageResource>(spaceInfo.storageId);
        if (!storage)
            continue;

        processStorage(storage, spaceInfo);
    }

    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    if (!success)
        NX_ERROR(this, "at_storageSpaceReply() - request failed");

    const auto requestKey = m_requests.take(handle);

    emit storageSpaceRecieved(requestKey.server, success, handle, reply);

    if (!success)
        return;

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo& serverInfo = m_serverInfo[requestKey.server];
    if (!serverInfo.fastInfoReady)
    {
        /* Immediately send usual request if we have received the fast one. */
        serverInfo.fastInfoReady = true;
        requestStorageSpace(requestKey.server);
    }

    /*
     * Reply can contain some storages that were instantiated after the server starts.
     * Therefore they will be absent in the resource pool, but user can add them manually.
     * Also we should manually remove these storages from pool if they were not added on the
     * server side and removed before getting into database.
     * This code should be executed if and only if server still exists and our request is actual.
     */
    QSet<QString> existingStorages;
    for (const QnStorageResourcePtr &storage: requestKey.server->getStorages())
        existingStorages.insert(storage->getUrl());

    for (const QnStorageSpaceData& spaceInfo: reply.storages)
    {
        /* Skip storages that are already exist. */
        if (existingStorages.contains(spaceInfo.url))
            continue;

        /* If we have just deleted a storage, we can receive it's space info from the previous request. Just skip it. */
        if (!spaceInfo.storageId.isNull())
            continue;

        QnClientStorageResourcePtr storage =
            QnClientStorageResource::newStorage(requestKey.server, spaceInfo.url);
        storage->setStorageType(spaceInfo.storageType);

        resourcePool()->addResource(storage);
        processStorage(storage, spaceInfo);

        existingStorages.insert(spaceInfo.url);
    }

    QSet<QString> receivedStorages;
    for (const QnStorageSpaceData& spaceInfo: reply.storages)
        receivedStorages.insert(spaceInfo.url);

    QnResourceList storagesToDelete;
    for (const QnStorageResourcePtr& storage: requestKey.server->getStorages())
    {
        /*
         * Skipping storages that are really present in the server resource pool.
         * They will be deleted on storageRemoved transaction.
         */
        QnClientStorageResourcePtr clientStorage = storage.dynamicCast<QnClientStorageResource>();
        NX_ASSERT(clientStorage, "Only client storage intances must exist on the client side.");
        if (clientStorage && clientStorage->isActive())
            continue;

        /* Skipping storages that are confirmed by server. */
        if (receivedStorages.contains(storage->getUrl()))
            continue;

        /* Removing other storages (e.g. external drives that were switched off. */
        storagesToDelete << storage;
    }

    resourcePool()->removeResources(storagesToDelete);

    auto replyProtocols = reply.storageProtocols.toSet();
    if (serverInfo.protocols != replyProtocols)
    {
        serverInfo.protocols = replyProtocols;
        emit serverProtocolsChanged(requestKey.server, replyProtocols);
    }
}
