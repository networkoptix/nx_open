// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_storage_manager.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_scan_info.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_media_server_manager.h>
#include <utils/common/delayed.h>

using namespace nx::vms::client::desktop;

namespace {

using namespace std::chrono;

// Delay between requests when the rebuild is running.
static constexpr auto kUpdateRebuildStatusDelay = 1s;

void processStorage(
    const QnClientStorageResourcePtr& storage,
    const nx::vms::api::StorageSpaceDataV1& spaceInfo)
{
    storage->setFreeSpace(spaceInfo.freeSpace);
    storage->setTotalSpace(spaceInfo.totalSpace);
    storage->setWritable(spaceInfo.isWritable);
    storage->setSpaceLimit(spaceInfo.reservedSpace);
    storage->setStatus(
        spaceInfo.isOnline ? nx::vms::api::ResourceStatus::online : nx::vms::api::ResourceStatus::offline);
    nx::vms::api::StorageRuntimeFlags runtimeFlags;
    if (spaceInfo.storageStatus.testFlag(nx::vms::api::StorageStatus::beingChecked))
        runtimeFlags |= nx::vms::api::StorageRuntimeFlag::beingChecked;
    if (spaceInfo.storageStatus.testFlag(nx::vms::api::StorageStatus::beingRebuilt))
        runtimeFlags |= nx::vms::api::StorageRuntimeFlag::beingRebuilt;
    if (spaceInfo.storageStatus.testFlag(nx::vms::api::StorageStatus::disabled))
        runtimeFlags |= nx::vms::api::StorageRuntimeFlag::disabled;
    if (spaceInfo.storageStatus.testFlag(nx::vms::api::StorageStatus::tooSmall))
        runtimeFlags |= nx::vms::api::StorageRuntimeFlag::tooSmall;
    if (spaceInfo.storageStatus.testFlag(nx::vms::api::StorageStatus::used))
        runtimeFlags |= nx::vms::api::StorageRuntimeFlag::used;
    storage->setRuntimeStatusFlags(runtimeFlags);
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

// Helper method to generate a callback, compatible with rest::ServerConenction::postJsonResult
// It attaches to an object with type `Class` and a method with a signature:
// `void someMethod(bool success, int handle, const TargetType& data)`
template<class Class, class TargetType> auto methodCallback(
    Class* object,
    void (Class::* method)(bool, rest::Handle, const TargetType&))
{
    NX_ASSERT(object);
    return nx::utils::guarded(object,
        [object, method](bool success, rest::Handle handle, nx::network::rest::JsonResult jsonData)
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
    std::function<void (bool, rest::Handle, const TargetType&)> callback)
{
    return
        [callback](bool success, rest::Handle handle, nx::network::rest::JsonResult jsonData)
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

} // namespace

struct ServerPoolInfo
{
    nx::vms::api::StorageScanInfo rebuildStatus{};
};

struct QnServerStorageManager::ServerInfo
{
    std::array<ServerPoolInfo, static_cast<int>(QnServerStoragesPool::Count)> storages{};
    QSet<std::string> protocols;
    bool fastInfoReady = false;
};

QnServerStorageManager::RequestKey::RequestKey(
    const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool)
    :
    server(server),
    pool(pool)
{
}

template<> QnServerStorageManager* Singleton<QnServerStorageManager>::s_instance = nullptr;

QnServerStorageManager::QnServerStorageManager(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnServerStorageManager::handleResourceAdded);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnServerStorageManager::handleResourceRemoved);

    const auto allServers = resourcePool()->servers();

    for (const auto& server: allServers)
        handleResourceAdded(server);

    connect(systemContext->serverRuntimeEventConnector(),
        &ServerRuntimeEventConnector::analyticsStorageParametersChanged,
        this,
        [this](const QnUuid& serverId)
        {
            const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
            if (server && isServerValid(server))
                emit activeMetadataStorageChanged(server);
        });
}

QnServerStorageManager::~QnServerStorageManager()
{
}

QnServerStorageManager* QnServerStorageManager::instance()
{
    return appContext()->currentSystemContext()->serverStorageManager();
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

    if (const auto storage = resource.objectCast<QnClientStorageResource>())
    {
        connect(storage.data(), &QnResource::statusChanged,
            this, &QnServerStorageManager::checkStoragesStatusInternal);

        connect(storage.data(), &QnResource::statusChanged, this, handleStorageChanged);

        // Free space is not used in the client but called quite often.
        //connect(storage.data(), &QnClientStorageResource::freeSpaceChanged,
        //    this, emitStorageChanged);

        connect(storage.data(), &QnClientStorageResource::totalSpaceChanged, this, emitStorageChanged);
        connect(storage.data(), &QnClientStorageResource::isWritableChanged, this, handleStorageChanged);

        connect(storage.data(), &QnClientStorageResource::isUsedForWritingChanged,
            this, handleStorageChanged);

        connect(storage.data(), &QnClientStorageResource::isBackupChanged, this, emitStorageChanged);
        connect(storage.data(), &QnClientStorageResource::isActiveChanged, this, emitStorageChanged);

        emit storageAdded(storage);
        checkStoragesStatusInternal(storage);
        return;
    }

    const auto server = resource.objectCast<QnMediaServerResource>();
    if (!server || server->hasFlags(Qn::fake) || server->hasFlags(Qn::cross_system))
        return;

    m_serverInfo.insert(server, ServerInfo());

    connect(server.data(), &QnMediaServerResource::statusChanged,
        this, &QnServerStorageManager::checkStoragesStatusInternal);

    connect(server.data(), &QnMediaServerResource::apiUrlChanged,
        this, &QnServerStorageManager::checkStoragesStatusInternal);

    connect(server.data(), &QnMediaServerResource::propertyChanged, this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            if (key != ResourcePropertyKey::Server::kMetadataStorageIdKey)
                return;

            updateActiveMetadataStorage(resource.objectCast<QnMediaServerResource>(),
                /*suppressNotificationSignal*/ true);
        });

    checkStoragesStatus(server);
};

void QnServerStorageManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto storage = resource.dynamicCast<QnStorageResource>())
    {
        storage->disconnect(this);
        checkStoragesStatusInternal(storage);
        emit storageRemoved(storage);

        const auto server = storage->getParentServer();
        if (server && activeMetadataStorage(server) == storage)
            updateActiveMetadataStorage(server);

        return;
    }

    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server.dynamicCast<QnFakeMediaServerResource>())
        return;

    server->disconnect(this);
    m_activeMetadataStorages.remove(server);
    m_serverInfo.remove(server);
}

void QnServerStorageManager::invalidateRequests()
{
    m_requests.clear();
}

bool QnServerStorageManager::isServerValid(const QnMediaServerResourcePtr& server) const
{
    // Server is invalid.
    if (!server)
        return false;

    // Server is not watched.
    if (!m_serverInfo.contains(server))
        return false;

    return server->getStatus() == nx::vms::api::ResourceStatus::online;
}

QSet<std::string> QnServerStorageManager::protocols(const QnMediaServerResourcePtr& server) const
{
    return m_serverInfo.contains(server)
        ? m_serverInfo[server].protocols
        : QSet<std::string>();
}

nx::vms::api::StorageScanInfo QnServerStorageManager::rebuildStatus(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool) const
{
    return m_serverInfo.contains(server)
        ? m_serverInfo[server].storages[(int) pool].rebuildStatus
        : nx::vms::api::StorageScanInfo();
}

bool QnServerStorageManager::rebuildServerStorages(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
{
    NX_VERBOSE(this, "Starting rebuild %1 of %2", (int)pool, server);
    if (!sendArchiveRebuildRequest(server, pool, RebuildAction::start))
        return false;

    ServerInfo& serverInfo = m_serverInfo[server];
    ServerPoolInfo& poolInfo = serverInfo.storages[static_cast<int>(pool)];
    poolInfo.rebuildStatus.state = nx::vms::api::RebuildState::full;
    NX_VERBOSE(this, "Initializing status of pool %1 to fullscan", (int)pool);

    emit serverRebuildStatusChanged(server, pool, poolInfo.rebuildStatus);

    return true;
}

bool QnServerStorageManager::cancelServerStoragesRebuild(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool)
{
    return sendArchiveRebuildRequest(server, pool, RebuildAction::cancel);
}

void QnServerStorageManager::checkStoragesStatus(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return;

    updateActiveMetadataStorage(server);

    if (!isServerValid(server))
        return;

    if (!systemContext()->accessController()->hasPowerUserPermissions())
        return;

    NX_VERBOSE(this, "Check storages status on server changes");
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Main);
    sendArchiveRebuildRequest(server, QnServerStoragesPool::Backup);
}

void QnServerStorageManager::checkStoragesStatusInternal(const QnResourcePtr& resource)
{
    checkStoragesStatus(getServerForResource(resource));
}

void QnServerStorageManager::saveStorages(const QnStorageResourceList& storages)
{
    auto messageBusConnection = systemContext()->messageBusConnection();

    if (!messageBusConnection)
        return;

    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(storages, apiStorages);

    messageBusConnection->getMediaServerManager(Qn::kSystemAccess)->saveStorages(
        apiStorages,
        [storages](int /*reqID*/, ec2::ErrorCode error)
        {
            if (error != ec2::ErrorCode::ok)
                return;

            for (const auto& storage: storages)
            {
                if (const auto clientStorage = storage.dynamicCast<QnClientStorageResource>())
                    clientStorage->setActive(true);
            }
        },
        this);

    invalidateRequests();
}

void QnServerStorageManager::deleteStorages(const nx::vms::api::IdDataList& ids)
{
    auto messageBusConnection = systemContext()->messageBusConnection();

    if (!messageBusConnection)
        return;

    messageBusConnection->getMediaServerManager(Qn::kSystemAccess)->removeStorages(
        ids, [](int /*requestId*/, ec2::ErrorCode) {});
    invalidateRequests();
}

bool QnServerStorageManager::sendArchiveRebuildRequest(
    const QnMediaServerResourcePtr& server, QnServerStoragesPool pool, RebuildAction action)
{
    if (!isServerValid(server))
        return false;

    const auto api = connectedServerApi();
    if (!api)
        return false;

    const auto poolStr = (pool == QnServerStoragesPool::Main)
        ? QString("main")
        : QString("backup");

    auto rebuildCallback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle handle,
            rest::ErrorOrData<nx::vms::api::StorageScanInfoFull> errorOrData)
        {
            if (success)
            {
                at_archiveRebuildReply(
                    success,
                    handle,
                    std::get<nx::vms::api::StorageScanInfoFull>(errorOrData));
            }
            else
            {
                const auto error = std::get<nx::network::rest::Result>(errorOrData);
                NX_ERROR(this, "Can't rebuld archive, error: %1, text: %2",
                    error.error, error.errorString);

                at_archiveRebuildReply(success, handle, {});
            }
        });

    int handle = 0;
    if (action == RebuildAction::showProgress)
    {
        handle = api->getArchiveRebuildProgress(
            server->getId(),
            poolStr,
            std::move(rebuildCallback),
            thread());
    }
    else if (action == RebuildAction::start)
    {
        handle = api->startArchiveRebuild(
            server->getId(),
            poolStr,
            std::move(rebuildCallback),
            thread());
    }
    else if (action == RebuildAction::cancel)
    {
        auto stopCallback = nx::utils::guarded(this,
            [this](bool success, rest::Handle handle, rest::ErrorOrEmpty errorOrEmpty)
            {
                if (const auto error = std::get_if<nx::network::rest::Result>(&errorOrEmpty))
                {
                    NX_ERROR(this, "Can't cancel archive rebuld, error: %1, text: %2",
                        error->error, error->errorString);
                }

                at_archiveRebuildReply(success, handle, {});
            });

        handle = api->stopArchiveRebuild(
            server->getId(),
            poolStr,
            std::move(stopCallback),
            thread());
    }

    NX_VERBOSE(this, "Send request %1 to rebuild pool %2 of %3 (action %4)",
        handle, (int)pool, server, action);
    if (handle <= 0)
        return false;

    if (action != RebuildAction::showProgress)
        invalidateRequests();

    m_requests.insert(handle, RequestKey(server, pool));
    return true;
}

void QnServerStorageManager::at_archiveRebuildReply(
    bool success, rest::Handle handle, const nx::vms::api::StorageScanInfoFull& reply)
{
    NX_VERBOSE(this, "Received archive rebuild reply %1, success %2, %3", handle, success, reply);
    if (!m_requests.contains(handle))
        return;

    const auto requestKey = m_requests.take(handle);
    NX_VERBOSE(this, "Target server %1, pool %2", requestKey.server, (int)requestKey.pool);

    // Whether server was removed from the pool.
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo& serverInfo = m_serverInfo[requestKey.server];
    ServerPoolInfo& poolInfo = serverInfo.storages[static_cast<int>(requestKey.pool)];

    nx::vms::api::StorageScanInfo replyInfo;
    if (reply.size() == 1)
        replyInfo = reply.begin()->second;

    Callback delayedCallback;
    if (replyInfo.state > nx::vms::api::RebuildState::none)
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
            [this, requestKey]() { requestStorageSpace(requestKey.server); };
    }

    executeDelayedParented(delayedCallback, kUpdateRebuildStatusDelay, this);

    if (!success)
        return;

    if (poolInfo.rebuildStatus != replyInfo)
    {
        const bool finished = (poolInfo.rebuildStatus.state == nx::vms::api::RebuildState::full
            && replyInfo.state == nx::vms::api::RebuildState::none);

        NX_VERBOSE(this, "Rebuild status changed, is finished: %1", finished);

        poolInfo.rebuildStatus = replyInfo;
        emit serverRebuildStatusChanged(requestKey.server, requestKey.pool, replyInfo);

        if (finished)
            emit serverRebuildArchiveFinished(requestKey.server, requestKey.pool);
    }
}

int QnServerStorageManager::requestStorageSpace(const QnMediaServerResourcePtr& server)
{
    if (!isServerValid(server) || !connection())
        return 0;

    nx::network::rest::Params params;
    const bool sendFastRequest = !m_serverInfo[server].fastInfoReady;
    if (sendFastRequest)
        params.insert("fast", QnLexical::serialized(true));

    const int handle = connectedServerApi()->getJsonResult(
        "/api/storageSpace",
        params,
        methodCallback(this, &QnServerStorageManager::at_storageSpaceReply),
        thread(),
        server->getId());

    if (!handle)
        return 0;

    m_requests.insert(handle, RequestKey(server, QnServerStoragesPool::Main));
    return handle;
}

int QnServerStorageManager::requestStorageStatus(
    const QnMediaServerResourcePtr& server,
    const QString& storageUrl,
    std::function<void(const StorageStatusReply&)> callback)
{
    if (!isServerValid(server) || !connection())
        return 0;

    return connectedServerApi()->getStorageStatus(
        server->getId(),
        storageUrl,
        [callback = std::move(callback)](
            bool success,
            rest::Handle /*requestId*/,
            rest::RestResultWithData<StorageStatusReply> result)
        {
            if (!success || result.error != nx::network::rest::Result::NoError)
            {
                callback(StorageStatusReply());
                return;
            }

            callback(result.data);
        },
        thread());
}

int QnServerStorageManager::requestRecordingStatistics(const QnMediaServerResourcePtr& server,
    qint64 bitrateAnalyzePeriodMs,
    std::function<void (bool, rest::Handle, const QnRecordingStatsReply&)> callback)
{
    if (!isServerValid(server) || !connection())
        return 0;

    nx::network::rest::Params params;
    params.insert("bitrateAnalyzePeriodMs", bitrateAnalyzePeriodMs);

    return connectedServerApi()->getJsonResult(
        "/api/recStats",
        params,
        simpleCallback(callback),
        thread(),
        server->getId());
}

void QnServerStorageManager::at_storageSpaceReply(
    bool success, rest::Handle handle, const nx::vms::api::StorageSpaceReply& reply)
{
    for (const auto& spaceInfo: reply.storages)
    {
        if (const auto storage =
            resourcePool()->getResourceById<QnClientStorageResource>(spaceInfo.storageId))
        {
            processStorage(storage, spaceInfo);
        }
    }

    if (!m_requests.contains(handle))
        return;

    if (!success)
        NX_ERROR(this, "at_storageSpaceReply() - request failed");

    const auto requestKey = m_requests.take(handle);

    emit storageSpaceRecieved(requestKey.server, success, handle, reply);

    if (!success)
        return;

    // Whether server was removed from the pool.
    if (!m_serverInfo.contains(requestKey.server))
        return;

    ServerInfo& serverInfo = m_serverInfo[requestKey.server];
    if (!serverInfo.fastInfoReady)
    {
        // Immediately send usual request if we have received the fast one.
        serverInfo.fastInfoReady = true;
        requestStorageSpace(requestKey.server);
    }

    // Reply can contain some storages that were instantiated after the server starts.
    // Therefore they will be absent in the resource pool, but user can add them manually.
    // Also we should manually remove these storages from the pool if they were not added on the
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
        const auto clientStorage = storage.dynamicCast<QnClientStorageResource>();
        NX_ASSERT(clientStorage, "Only client storage intances must exist on the client side.");
        if (clientStorage && clientStorage->isActive())
            continue;

        // Skipping storages that are confirmed by server.
        if (receivedStorageUrls.contains(storage->getUrl()))
            continue;

        // Removing other storages (e.g. external drives that were switched off).
        storagesToDelete.push_back(storage);
    }

    NX_DEBUG(this, "Remove storages");
    resourcePool()->removeResources(storagesToDelete);

    auto replyProtocols = QSet(
        reply.storageProtocols.cbegin(),
        reply.storageProtocols.cend());
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
    const QnStorageResourcePtr& storage, bool suppressNotificationSignal)
{
    if (!NX_ASSERT(server))
        return;

    if (m_activeMetadataStorages.value(server) == storage)
        return;

    m_activeMetadataStorages[server] = storage;

    NX_VERBOSE(this, "Metadata storage state changed on server %1 (%2)", server->getName(),
        server->getId().toSimpleString());

    if (!suppressNotificationSignal)
        emit activeMetadataStorageChanged(server);
}

void QnServerStorageManager::updateActiveMetadataStorage(
    const QnMediaServerResourcePtr& server, bool suppressNotificationSignal)
{
    setActiveMetadataStorage(server, calculateActiveMetadataStorage(server),
        suppressNotificationSignal);
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
