#include "resource_pool_peer_manager.h"

#include <functional>
#include <atomic>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/delayed.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

#include "internet_only_peer_manager.h"
#include "../downloader.h"

namespace nx::vms::common::p2p::downloader {

class ResourcePoolPeerManager::Private
{
    ResourcePoolPeerManager* const q = nullptr;

public:
    QScopedPointer<InternetOnlyPeerManager> internetPeerManger;
    QHash<QnUuid, rest::QnConnectionPtr> directConnectionByServerId;
    PeerSelector peerSelector;

    QnMutex mutex;
    std::atomic<rest::Handle> nextRequestId = 1;
    QHash<rest::Handle, std::function<void()>> cancelFunctionByRequestId;
    bool allowIndirectInternetRequests = false;

public:
    Private(ResourcePoolPeerManager* q):
        q(q)
    {
    }

    void saveCanceller(rest::Handle handle, std::function<void()>&& canceller)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        cancelFunctionByRequestId.insert(handle, std::move(canceller));
    }

    bool removeCanceller(rest::Handle handle)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        return cancelFunctionByRequestId.remove(handle) == 1;
    }

    QList<PeerSelector::PeerInformation> otherPeersInfos() const
    {
        using PeerInformation = PeerSelector::PeerInformation;

        QList<PeerInformation> result;

        const auto& selfId = q->commonModule()->moduleGUID();

        for (const auto& server: q->commonModule()->resourcePool()->getAllServers(Qn::Online))
        {
            if (const auto& id = server->getId(); id != selfId)
                result.append(PeerInformation{id, server->getSystemInfo()});
        }

        for (const auto& id: directConnectionByServerId.keys())
        {
            if (!std::any_of(result.begin(), result.end(),
                [&id](const PeerInformation& info) { return info.id == id; }))
            {
                result.append(PeerInformation{id, nx::vms::api::SystemInformation()});
            }
        }

        return result;
    }

    QList<QnUuid> otherPeersIds() const
    {
        QList<QnUuid> result;

        for (const auto& info: otherPeersInfos())
            result.append(info.id);

        return result;
    }
};

ResourcePoolPeerManager::ResourcePoolPeerManager(
    QnCommonModule* commonModule,
    const PeerSelector& peerSelector)
    :
    ResourcePoolPeerManager(AllCapabilities, commonModule, peerSelector)
{
}

ResourcePoolPeerManager::ResourcePoolPeerManager(
    Capabilities capabilities,
    QnCommonModule* commonModule,
    const PeerSelector& peerSelector)
    :
    AbstractPeerManager(capabilities),
    QnCommonModuleAware(commonModule),
    d(new Private(this))
{
    d->internetPeerManger.reset(new InternetOnlyPeerManager());
    d->peerSelector = peerSelector;
}

ResourcePoolPeerManager::~ResourcePoolPeerManager()
{
}

QString ResourcePoolPeerManager::peerString(const QnUuid& peerId) const
{
    QString result;

    const QString proxy(d->allowIndirectInternetRequests ? "proxy " : "");
    const auto& server = resourcePool()->getResourceById<QnMediaServerResource>(peerId);
    if (server)
    {
        result = QStringLiteral("%1%2 (%3 %4)").arg(
            proxy,
            server->getName(),
            server->getPrimaryAddress().toString(),
            server->getId().toString());
    }
    else
    {
        result = QStringLiteral("Unknown server %1%2").arg(proxy, peerId.toString());
    }

    return result;
}

QList<QnUuid> ResourcePoolPeerManager::getAllPeers() const
{
    return d->otherPeersIds();
}

QList<QnUuid> ResourcePoolPeerManager::peers() const
{
    return d->peerSelector.selectPeers(d->otherPeersInfos());
}

int ResourcePoolPeerManager::distanceTo(const QnUuid& peerId) const
{
    const auto& connection = commonModule()->ec2Connection();
    if (!connection)
        return -1;

    auto distance = std::numeric_limits<int>::max();
    connection->routeToPeerVia(peerId, &distance, /*address*/ nullptr);
    return distance;
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

void ResourcePoolPeerManager::setIndirectInternetRequestsAllowed(bool allow)
{
    d->allowIndirectInternetRequests = allow;
}

rest::Handle ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url,
    FileInfoCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    const auto localRequestId = d->nextRequestId.fetch_add(1);

    if (d->allowIndirectInternetRequests)
    {
        d->saveCanceller(localRequestId, nullptr);

        std::async(std::launch::async,
            [this, fileName, url, peerId, callback, localRequestId, thread = thread()]()
            {
                if (!d->removeCanceller(localRequestId))
                    return;

                const auto& server =
                    resourcePool()->getResourceById<QnMediaServerResource>(peerId);
                const bool hasInternet =
                    !server || server->getServerFlags().testFlag(vms::api::SF_HasPublicIP);

                executeInThread(thread,
                    [fileName, url, hasInternet, callback, localRequestId]()
                    {
                        if (hasInternet && url.isValid())
                            callback(true, localRequestId, FileInformation(fileName));
                        else
                            callback(false, localRequestId, FileInformation());
                    });
            });

        return localRequestId;
    }

    const auto handleReply =
        [this, callback, localRequestId](
            bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            d->removeCanceller(localRequestId);

            if (!success)
                return callback(success, localRequestId, FileInformation());

            const auto& fileInfo = result.deserialized<FileInformation>();
            callback(success, localRequestId, fileInfo);
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
            bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            d->removeCanceller(localRequestId);

            if (!success)
                return callback(success, localRequestId, QVector<QByteArray>());

            const auto& checksums = result.deserialized<QVector<QByteArray>>();
            callback(success, localRequestId, checksums);
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
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    const auto localRequestId = d->nextRequestId.fetch_add(1);

    const auto handleReply =
        [this, callback = std::move(callback), localRequestId](
            bool success,
            rest::Handle /*requestId*/,
            QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            d->removeCanceller(localRequestId);
            return callback(success, localRequestId, result);
        };


    rest::Handle handle = -1;
    if (d->allowIndirectInternetRequests)
    {
        handle = connection->downloadFileChunkFromInternet(
            fileName, url, chunkIndex, chunkSize, handleReply, thread());
    }
    else
    {
        handle = connection->downloadFileChunk(fileName, chunkIndex, handleReply, thread());
    }

    if (handle < 0)
        return -1;

    d->saveCanceller(
        localRequestId,
        [connection, handle]() { connection->cancelRequest(handle); });

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
    {
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (const auto& directConnection = d->directConnectionByServerId.value(peerId))
            return directConnection;
    }

    if (const auto& server = getServer(peerId))
        return server->restConnection();

    return {};
}

ResourcePoolProxyPeerManager::ResourcePoolProxyPeerManager(
    QnCommonModule* commonModule,
    const PeerSelector& peerSelector)
    :
    ResourcePoolPeerManager(Capabilities(FileInfo | DownloadChunk), commonModule, peerSelector)
{
    setIndirectInternetRequestsAllowed(true);
}

} // namespace nx::vms::common::p2p::downloader
