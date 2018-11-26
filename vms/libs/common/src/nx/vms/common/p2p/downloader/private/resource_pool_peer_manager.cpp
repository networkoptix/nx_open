#include "resource_pool_peer_manager.h"

#include <functional>
#include <atomic>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

#include "peer_selection/abstract_peer_selector.h"
#include "internet_only_peer_manager.h"
#include "../downloader.h"

namespace nx::vms::common::p2p::downloader {

namespace {

PeerInformationList otherPeersInfos(QnCommonModule* commonModule)
{
    PeerInformationList result;

    const auto& selfId = commonModule->moduleGUID();

    for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
    {
        if (const auto& id = server->getId(); id != selfId)
            result.append(PeerInformation{server->getSystemInfo(), id});
    }

    return result;
}

QList<QnUuid> otherPeersIds(QnCommonModule* commonModule)
{
    QList<QnUuid> result;

    for (const auto& info: otherPeersInfos(commonModule))
        result.append(info.id);

    return result;
}

} // namespace

nx::network::http::AsyncHttpClientPtr createHttpClient()
{
    static const int kDownloadRequestTimeoutMs = 10 * 60 * 1000;

    auto httpClient = nx::network::http::AsyncHttpClient::create();
    httpClient->setResponseReadTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setSendTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setMessageBodyReadTimeoutMs(kDownloadRequestTimeoutMs);

    return httpClient;
}

class ResourcePoolPeerManager::Private
{
public:
    QScopedPointer<InternetOnlyPeerManager> internetPeerManger;
    QHash<QnUuid, rest::QnConnectionPtr> directConnectionByServerId;
    AbstractPeerSelectorPtr peerSelector;
    bool isClient = false;

    QnMutex mutex;
    std::atomic<rest::Handle> nextRequestId = 1;
    QHash<rest::Handle, std::function<void()>> cancelFunctionByRequestId;

public:
    void saveCanceller(rest::Handle handle, std::function<void()>&& canceller)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        cancelFunctionByRequestId.insert(handle, std::move(canceller));
    }

    void removeCanceller(rest::Handle handle)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        cancelFunctionByRequestId.remove(handle);
    }
};

ResourcePoolPeerManager::ResourcePoolPeerManager(
    QnCommonModule* commonModule,
    AbstractPeerSelectorPtr peerSelector,
    bool isClient)
    :
    QnCommonModuleAware(commonModule),
    d(new Private())
{
    d->internetPeerManger.reset(new InternetOnlyPeerManager());
    d->peerSelector = std::move(peerSelector);
    d->isClient = isClient;
}

ResourcePoolPeerManager::~ResourcePoolPeerManager()
{
}

QnUuid ResourcePoolPeerManager::selfId() const
{
    return commonModule()->moduleGUID();
}

QString ResourcePoolPeerManager::peerString(const QnUuid& peerId) const
{
    QString result;

    const auto& server = resourcePool()->getResourceById<QnMediaServerResource>(peerId);
    if (server)
    {
        result = QStringLiteral("%1 (%2 %3)").arg(
            server->getName(),
            server->getPrimaryAddress().toString(),
            server->getId().toString());
    }
    else
    {
        result = QStringLiteral("Unknown server %1").arg(peerId.toString());
    }

    if (peerId == selfId())
        result += QStringLiteral(" (self)");

    return result;
}

QList<QnUuid> ResourcePoolPeerManager::getAllPeers() const
{
    return otherPeersIds(commonModule());
}

int ResourcePoolPeerManager::distanceTo(const QnUuid& peerId) const
{
    if (peerId == selfId())
        return 0;

    const auto& connection = commonModule()->ec2Connection();
    if (!connection)
        return -1;

    auto distance = std::numeric_limits<int>::max();
    connection->routeToPeerVia(peerId, &distance, /*address*/ nullptr);
    return distance;
}

bool ResourcePoolPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    // Note: peerId can be a client id..
    if (d->isClient)
        return true;

    const auto& server = getServer(peerId);
    if (!NX_ASSERT(server))
        return false;

    return server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
}

bool ResourcePoolPeerManager::hasAccessToTheUrl(const QString& url) const
{
    if (url.isEmpty())
        return false;

    return Downloader::validate(url, /* onlyConnectionCheck */ true, /* expectedSize */ 0);
}

void ResourcePoolPeerManager::setServerDirectConnection(
    const QnUuid& id, const rest::QnConnectionPtr& connection)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (connection)
        d->directConnectionByServerId.insert(id, connection);
    else
        d->directConnectionByServerId.remove(id);
}

rest::Handle ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId, const QString& fileName, FileInfoCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    const auto localRequestId = d->nextRequestId.fetch_add(1);

    const auto handleReply =
        [this, callback, localRequestId](
            bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->removeCanceller(localRequestId);

            if (!success)
                return callback(success, handle, FileInformation());

            const auto& fileInfo = result.deserialized<FileInformation>();
            callback(success, handle, fileInfo);
        };

    const auto handle = connection->fileDownloadStatus(fileName, handleReply, thread());
    if (handle < 0)
        return -1;

    d->saveCanceller(
        localRequestId,
        [connection, handle]() { connection->cancelRequest(handle); });

    return localRequestId;
}

rest::Handle ResourcePoolPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    const auto localRequestId = d->nextRequestId.fetch_add(1);

    const auto handleReply =
        [this, callback, localRequestId](
            bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->removeCanceller(localRequestId);

            if (!success)
                return callback(success, handle, QVector<QByteArray>());

            const auto& checksums = result.deserialized<QVector<QByteArray>>();
            callback(success, handle, checksums);
        };

    const auto handle = connection->fileChunkChecksums(fileName, handleReply, thread());
    if (handle < 0)
        return -1;

    d->saveCanceller(
        localRequestId,
        [connection, handle]() { connection->cancelRequest(handle); });

    return localRequestId;
}

rest::Handle ResourcePoolPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    const auto localRequestId = d->nextRequestId.fetch_add(1);

    const auto handleReply =
        [this, callback = std::move(callback), localRequestId](
            bool success,
            rest::Handle requestId,
            QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            d->removeCanceller(localRequestId);
            return callback(success, requestId, result);
        };


    const auto handle = connection->downloadFileChunk(
        fileName, chunkIndex, handleReply, thread());
    if (handle < 0)
        return -1;

    d->saveCanceller(
        localRequestId,
        [connection, handle]() { connection->cancelRequest(handle); });

    return localRequestId;
}

rest::Handle ResourcePoolPeerManager::downloadChunkFromInternet(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    const auto localRequestId = d->nextRequestId.fetch_add(1);
    rest::Handle handle = -1;
    std::function<void()> cancelFunction;

    if (peerId == selfId())
    {
        const auto handleReply =
            [this, callback = std::move(callback), localRequestId](
                bool result,
                rest::Handle handle,
                const QByteArray& data)
            {
                d->removeCanceller(localRequestId);
                callback(result, handle, data);
            };

        handle = d->internetPeerManger->downloadChunkFromInternet(
            QnUuid(), fileName, url, chunkIndex, chunkSize, handleReply);

        cancelFunction =
            [this, handle]() { d->internetPeerManger->cancelRequest(QnUuid(), handle); };
    }
    else
    {
        const auto& connection = getConnection(peerId);
        if (!connection)
            return -1;

        const auto handleReply =
            [this, callback = std::move(callback), localRequestId](
                bool success,
                rest::Handle requestId,
                QByteArray result,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                d->removeCanceller(localRequestId);
                callback(success, requestId, success ? result : QByteArray());
            };

        handle = connection->downloadFileChunkFromInternet(
            fileName, url, chunkIndex, chunkSize, handleReply, thread());

        cancelFunction = [connection, handle]() { connection->cancelRequest(handle); };
    }

    if (handle < 0)
        return -1;

    d->saveCanceller(localRequestId, std::move(cancelFunction));

    return localRequestId;
}

void ResourcePoolPeerManager::cancelRequest(const QnUuid& /*peerId*/, rest::Handle handle)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    if (const auto& cancelFunction = d->cancelFunctionByRequestId.take(handle))
        cancelFunction();
}

QnMediaServerResourcePtr ResourcePoolPeerManager::getServer(const QnUuid& peerId) const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(peerId);
}

rest::QnConnectionPtr ResourcePoolPeerManager::getConnection(const QnUuid& peerId) const
{
    if (!NX_ASSERT(peerId != selfId()))
        return {};

    {
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (const auto& directConnection = d->directConnectionByServerId.value(peerId))
            return directConnection;
    }

    if (const auto& server = getServer(peerId))
        return server->restConnection();

    return {};
}

QList<QnUuid> ResourcePoolPeerManager::peers() const
{
    return d->peerSelector->peers(otherPeersInfos(commonModule()));
}

ResourcePoolPeerManagerFactory::ResourcePoolPeerManagerFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

AbstractPeerManager* ResourcePoolPeerManagerFactory::createPeerManager(
    FileInformation::PeerSelectionPolicy peerPolicy,
    const QList<QnUuid>& additionalPeers)
{
    auto&& peerSelector = createPeerSelector(peerPolicy, additionalPeers, commonModule());
    return new ResourcePoolPeerManager(commonModule(), std::move(peerSelector));
}

} // namespace nx::vms::common::p2p::downloader
