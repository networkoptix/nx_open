#include "server_storage_manager.h"

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/storage_space_reply.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/client_storage_resource.h>

#include <utils/common/delayed.h>

namespace {
    /** Delay between requests when the rebuild is running. */
    const int updateRebuildStatusDelayMs = 500;

    /** Delay between requests when the backup is running. */
    const int updateBackupStatusDelayMs = 500;

    void processStorage(const QnClientStorageResourcePtr &storage, const QnStorageSpaceData &spaceInfo) {
        storage->setFreeSpace(spaceInfo.freeSpace);
        storage->setTotalSpace(spaceInfo.totalSpace);
        storage->setWritable(spaceInfo.isWritable);
    }
}

struct ServerPoolInfo {
    int archiveRebuildRequestHandle;
    QnStorageScanData rebuildStatus;

    ServerPoolInfo()
        : archiveRebuildRequestHandle(0)
        , rebuildStatus()
    {}
};

struct QnServerStorageManager::ServerInfo {
    std::array<ServerPoolInfo, static_cast<int>(QnServerStoragesPool::Count)> storages;
    QSet<QString> protocols;
    int backupRequestHandle;
    int storageSpaceRequestHandle;
    QnBackupStatusData backupStatus;

    ServerInfo()
        : storages()
        , protocols()
        , backupRequestHandle(0)
        , storageSpaceRequestHandle(0)
        , backupStatus()
    {}
};

QnServerStorageManager::RequestKey::RequestKey()
    : server(QnMediaServerResourcePtr())
    , pool(QnServerStoragesPool::Main)
{}

QnServerStorageManager::RequestKey::RequestKey(const QnMediaServerResourcePtr &server, QnServerStoragesPool pool)
    : server(server)
    , pool(pool)
{}


QnServerStorageManager::QnServerStorageManager( QObject *parent )
    : base_type(parent)
{

    auto getServerForResource = [](const QnResourcePtr &resource) -> QnMediaServerResourcePtr {
        if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
            return server;
        if (QnStorageResourcePtr storage = resource.dynamicCast<QnStorageResource>())
            return storage->getParentServer();
        return QnMediaServerResourcePtr();
    };

    auto checkStoragesStatusInternal = [this, getServerForResource](const QnResourcePtr &resource) {
        checkStoragesStatus(getServerForResource(resource));
    };


    auto resourceAdded = [this, checkStoragesStatusInternal](const QnResourcePtr &resource) {

        auto emitStorageChanged = [this](const QnResourcePtr &resource) {
            emit storageChanged(resource.dynamicCast<QnStorageResource>());
        };

        if (const QnClientStorageResourcePtr &storage = resource.dynamicCast<QnClientStorageResource>()) {
            connect(storage, &QnResource::statusChanged, this, checkStoragesStatusInternal);
            //connect(storage, &QnClientStorageResource::freeSpaceChanged,    this, emitStorageChanged); // free space is not used in the client but called quite often
            connect(storage, &QnClientStorageResource::totalSpaceChanged,   this, emitStorageChanged);
            connect(storage, &QnClientStorageResource::isWritableChanged,   this, emitStorageChanged);

            emit storageAdded(storage);
            checkStoragesStatusInternal(storage);
            return;
        }

        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server || QnMediaServerResource::isFakeServer(server))
            return;

        m_serverInfo.insert(server, ServerInfo());

        connect(server,   &QnMediaServerResource::statusChanged,  this,   checkStoragesStatusInternal);
        connect(server,   &QnMediaServerResource::apiUrlChanged,  this,   checkStoragesStatusInternal);
        checkStoragesStatus(server);
    };

    auto resourceRemoved = [this, checkStoragesStatusInternal](const QnResourcePtr &resource) {
        if (const QnStorageResourcePtr &storage = resource.dynamicCast<QnStorageResource>()) {
            disconnect(storage, nullptr, this, nullptr);
            checkStoragesStatusInternal(storage);
            emit storageRemoved(storage);
            return;
        }

        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server || QnMediaServerResource::isFakeServer(server))
            return;

        m_serverInfo.remove(server);
        disconnect(server, nullptr, this, nullptr);
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,      this, resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,    this, resourceRemoved);

    for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers())
        resourceAdded(server);
}

QnServerStorageManager::~QnServerStorageManager() {

}

bool QnServerStorageManager::isServerValid( const QnMediaServerResourcePtr &server ) const {
    /* Server is invalid. */
    if (!server || !server->apiConnection())
        return false;

    /* Server is not watched. */
    if (!m_serverInfo.contains(server))
        return false;

    return server->getStatus() == Qn::Online;
}

QSet<QString> QnServerStorageManager::protocols( const QnMediaServerResourcePtr &server ) const {
    if (!m_serverInfo.contains(server))
        return QSet<QString>();

    return m_serverInfo[server].protocols;
}

QnStorageScanData QnServerStorageManager::rebuildStatus( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool ) const {
    if (!m_serverInfo.contains(server))
        return QnStorageScanData();

    return m_serverInfo[server].storages[static_cast<int>(pool)].rebuildStatus;
}

QnBackupStatusData QnServerStorageManager::backupStatus( const QnMediaServerResourcePtr &server) const {
    if (!m_serverInfo.contains(server))
        return QnBackupStatusData();

    return m_serverInfo[server].backupStatus;
}

bool QnServerStorageManager::rebuildServerStorages( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool ) {
    return sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Start);
}

bool QnServerStorageManager::cancelServerStoragesRebuild( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool ) {
    return sendArchiveRebuildRequest(server, pool, Qn::RebuildAction_Cancel);
}

bool QnServerStorageManager::backupServerStorages( const QnMediaServerResourcePtr &server ) {
    return sendBackupRequest(server, Qn::BackupAction_Start);
}

bool QnServerStorageManager::cancelBackupServerStorages( const QnMediaServerResourcePtr &server ) {
    return sendBackupRequest(server, Qn::BackupAction_Cancel);
}

void QnServerStorageManager::checkStoragesStatus( const QnMediaServerResourcePtr &server ) {
    if (!isServerValid(server))
        return;

    sendArchiveRebuildRequest(server, QnServerStoragesPool::Main);
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Backup);
    sendBackupRequest(server);
}


//TODO: #GDM SafeMode
void QnServerStorageManager::saveStorages(const QnStorageResourceList &storages ) {
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    if (!conn)
        return;

    conn->getMediaServerManager()->saveStorages(storages, this, [] {});
}

//TODO: #GDM SafeMode
void QnServerStorageManager::deleteStorages(const ec2::ApiIdDataList &ids ) {
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    if (!conn)
        return;

    conn->getMediaServerManager()->removeStorages(ids, this, [] {});
}

bool QnServerStorageManager::sendArchiveRebuildRequest( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, Qn::RebuildAction action ) {
    if (!isServerValid(server))
        return false;

    int handle = server->apiConnection()->doRebuildArchiveAsync(
        action,
        pool == QnServerStoragesPool::Main,
        this,
        SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));
    if (handle <= 0)
        return false;

    m_requests.insert(handle, RequestKey(server, pool));
    m_serverInfo[server].storages[static_cast<int>(pool)].archiveRebuildRequestHandle = handle;
    return true;
}

void QnServerStorageManager::at_archiveRebuildReply( int status, const QnStorageScanData &reply, int handle ) {
    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo &serverInfo = m_serverInfo[requestKey.server];
    ServerPoolInfo &poolInfo = serverInfo.storages[static_cast<int>(requestKey.pool)];

    if (poolInfo.archiveRebuildRequestHandle != handle)
        return;

    poolInfo.archiveRebuildRequestHandle = 0;

    if (reply.state > Qn::RebuildState_None || status != 0)
        executeDelayed([this, requestKey]{ sendArchiveRebuildRequest(requestKey.server, requestKey.pool); }, updateRebuildStatusDelayMs);
    else
        executeDelayed([this, requestKey]{ sendStorageSpaceRequest(requestKey.server); }, updateRebuildStatusDelayMs);

    if(status != 0)
        return;

    if (poolInfo.rebuildStatus != reply) {
        bool finished = (poolInfo.rebuildStatus.state == Qn::RebuildState_FullScan && reply.state == Qn::RebuildState_None);

        poolInfo.rebuildStatus = reply;
        emit serverRebuildStatusChanged(requestKey.server, requestKey.pool, reply);

        if (finished)
            emit serverRebuildArchiveFinished(requestKey.server, requestKey.pool);
    }

}

bool QnServerStorageManager::sendBackupRequest( const QnMediaServerResourcePtr &server, Qn::BackupAction action ) {
    if (!isServerValid(server))
        return false;

    int handle = server->apiConnection()->backupControlActionAsync(
        action,
        this,
        SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
    if (handle <= 0)
        return false;

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    m_serverInfo[server].backupRequestHandle = handle;
    return true;
}

void QnServerStorageManager::at_backupStatusReply( int status, const QnBackupStatusData &reply, int handle ) {
    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo &serverInfo = m_serverInfo[requestKey.server];
    if (serverInfo.backupRequestHandle != handle)
        return;

    serverInfo.backupRequestHandle = 0;

    if (reply.state == Qn::BackupState_InProgress || status != 0)
        executeDelayed([this, requestKey]{ sendBackupRequest(requestKey.server); }, updateBackupStatusDelayMs);

    if(status != 0)
        return;

    if (serverInfo.backupStatus != reply) {
        bool finished = (serverInfo.backupStatus.state == Qn::BackupState_InProgress && reply.state == Qn::BackupState_None);

        serverInfo.backupStatus = reply;
        emit serverBackupStatusChanged(requestKey.server, reply);

        if (finished)
            emit serverBackupFinished(requestKey.server);
    }
}

bool QnServerStorageManager::sendStorageSpaceRequest( const QnMediaServerResourcePtr &server) {
    if (!isServerValid(server))
        return false;

    int handle = server->apiConnection()->getStorageSpaceAsync(
        this,
        SLOT(at_storageSpaceReply(int, const QnStorageSpaceReply &, int)));
    if (handle <= 0)
        return false;

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    m_serverInfo[server].storageSpaceRequestHandle = handle;
    return true;
}

void QnServerStorageManager::at_storageSpaceReply( int status, const QnStorageSpaceReply &reply, int handle ) {

    for (const QnStorageSpaceData &spaceInfo: reply.storages) {
        QnClientStorageResourcePtr storage = qnResPool->getResourceById<QnClientStorageResource>(spaceInfo.storageId);
        if (!storage)
            continue;

        processStorage(storage, spaceInfo);
    }

    /* Requests were invalidated. */
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);

    /* Server was removed from pool. */
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo &serverInfo = m_serverInfo[requestKey.server];

    if (serverInfo.storageSpaceRequestHandle != handle)
        return;

    serverInfo.storageSpaceRequestHandle = 0;

    if(status != 0)
        return;

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

    for (const QnStorageSpaceData &spaceInfo: reply.storages) {
        /* Skip storages that are already exist. */
        if (existingStorages.contains(spaceInfo.url))
            continue;

        Q_ASSERT_X(spaceInfo.storageId.isNull(), Q_FUNC_INFO, "We should process only non-pool storages here");
        if (!spaceInfo.storageId.isNull())
            continue;

        QnClientStorageResourcePtr storage = QnClientStorageResource::newStorage(requestKey.server, spaceInfo.url);
        storage->setStorageType(spaceInfo.storageType);

        processStorage(storage, spaceInfo);
        qnResPool->addResource(storage);

        existingStorages.insert(spaceInfo.url);
    }

    QSet<QString> receivedStorages;
    for (const QnStorageSpaceData &spaceInfo: reply.storages)
        receivedStorages.insert(spaceInfo.url);

    QnResourceList storagesToDelete;
    for (const QnStorageResourcePtr &storage: requestKey.server->getStorages()) {
        if (receivedStorages.contains(storage->getUrl()))
            continue;
        storagesToDelete << storage;
    }

    qnResPool->removeResources(storagesToDelete);

    auto replyProtocols = reply.storageProtocols.toSet();
    if (serverInfo.protocols != replyProtocols) {
        serverInfo.protocols = replyProtocols;
        emit serverProtocolsChanged(requestKey.server, replyProtocols);
    }
}

