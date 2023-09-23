// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_peer_manager.h"

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/delayed.h>

#include "../downloader.h"
#include "internet_only_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

namespace {

template<typename Promise>
auto makeCanceller(Promise promise, const rest::ServerConnectionPtr& connection, rest::Handle handle)
{
    return [promise, connection, handle]()
        {
            connection->cancelRequest(handle);
            AbstractPeerManager::setPromiseValueIfEmpty(promise, {});
        };
}

} // namespace

class ResourcePoolPeerManager::Private
{
    ResourcePoolPeerManager* const q = nullptr;

public:
    QHash<QnUuid, rest::ServerConnectionPtr> directConnectionByServerId;
    PeerSelector peerSelector;

    nx::Mutex mutex;
    bool allowIndirectInternetRequests = false;

public:
    Private(ResourcePoolPeerManager* q):
        q(q)
    {
    }

    QList<PeerSelector::PeerInformation> otherPeersInfos() const
    {
        using PeerInformation = PeerSelector::PeerInformation;

        QList<PeerInformation> result;

        const auto& selfId = q->peerId();

        for (const auto& server:
            q->resourcePool()->getAllServers(nx::vms::api::ResourceStatus::online))
        {
            if (const auto& id = server->getId(); id != selfId)
                result.append(PeerInformation{id, server->getOsInfo()});
        }

        for (const auto& id: directConnectionByServerId.keys())
        {
            if (!std::any_of(result.begin(), result.end(),
                [&id](const PeerInformation& info) { return info.id == id; }))
            {
                result.append(PeerInformation{id, {}});
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
    SystemContext* systemContext,
    const PeerSelector& peerSelector)
    :
    ResourcePoolPeerManager(AllCapabilities, systemContext, peerSelector)
{
}

ResourcePoolPeerManager::ResourcePoolPeerManager(
    Capabilities capabilities,
    SystemContext* systemContext,
    const PeerSelector& peerSelector)
    :
    AbstractPeerManager(capabilities),
    SystemContextAware(systemContext),
    d(new Private(this))
{
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
        result = nx::format("%1%2 (%3 %4)").args(
            proxy,
            server->getName(),
            server->getPrimaryAddress(),
            server->getId());
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
    auto otherInfo = d->otherPeersInfos();
    return d->peerSelector.selectPeers(otherInfo);
}

int ResourcePoolPeerManager::distanceTo(const QnUuid& peerId) const
{
    const auto& connection = messageBusConnection();
    if (!connection)
        return -1;

    auto distance = std::numeric_limits<int>::max();
    connection->routeToPeerVia(peerId, &distance, /*address*/ nullptr);
    return distance;
}

void ResourcePoolPeerManager::setServerDirectConnection(
    const QnUuid& id, const rest::ServerConnectionPtr& connection)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (connection)
        d->directConnectionByServerId.insert(id, connection);
    else
        d->directConnectionByServerId.remove(id);
}

void ResourcePoolPeerManager::clearServerDirectConnections()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->directConnectionByServerId.clear();
}

void ResourcePoolPeerManager::setIndirectInternetRequestsAllowed(bool allow)
{
    d->allowIndirectInternetRequests = allow;
}

AbstractPeerManager::RequestContextPtr<FileInformation> ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<FileInformation>>>();

    // Trying to download file from the Internet using some server as a proxy.
    if (d->allowIndirectInternetRequests)
    {
        const auto server = resourcePool()->getResourceById<QnMediaServerResource>(peerId);

        // If there is no corresponding server in the resources pool, then it is a direct connection
        // to the incompatible server (which is not updated yet, for example). We cannot know for
        // sure if it has internet or no, so trying to use it anyway.
        const bool hasInternet = !server || server->hasInternetAccess();

        if (hasInternet && url.isValid())
            setPromiseValueIfEmpty(promise, {FileInformation(fileName)});
        else
            setPromiseValueIfEmpty(promise, {});

        return std::make_unique<BaseRequestContext<FileInformation>>(promise->get_future());
    }

    auto handleReply =
        [promise](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            if (success)
                setPromiseValueIfEmpty(promise, {result.deserialized<FileInformation>()});
            else
                setPromiseValueIfEmpty(promise, {});
        };

    const auto handle = connection->fileDownloadStatus(peerId, fileName, std::move(handleReply));
    if (handle < 0)
        return {};

    return std::make_unique<BaseRequestContext<FileInformation>>(
        promise->get_future(), makeCanceller(promise, connection, handle));
}

AbstractPeerManager::RequestContextPtr<QVector<QByteArray>> ResourcePoolPeerManager::requestChecksums(
    const QnUuid& peerId, const QString& fileName)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<QVector<QByteArray>>>>();

    const auto handleReply =
        [promise](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            if (success)
                setPromiseValueIfEmpty(promise, {result.deserialized<QVector<QByteArray>>()});
            else
                setPromiseValueIfEmpty(promise, {});
        };

    const auto handle = connection->fileChunkChecksums(peerId, fileName, handleReply);
    if (handle < 0)
        return {};

    return std::make_unique<BaseRequestContext<QVector<QByteArray>>>(
        promise->get_future(), makeCanceller(promise, connection, handle));
}

AbstractPeerManager::RequestContextPtr<nx::Buffer> ResourcePoolPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    qint64 fileSize)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<nx::Buffer>>>();

    const auto handleReply =
        [promise](
            bool success,
            rest::Handle /*requestId*/,
            QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            if (success)
                setPromiseValueIfEmpty(promise, {result});
            else
                setPromiseValueIfEmpty(promise, {});
        };

    rest::Handle handle = -1;
    if (d->allowIndirectInternetRequests)
    {
        handle = connection->downloadFileChunkFromInternet(
            peerId, fileName, url, chunkIndex, chunkSize, fileSize, handleReply);
    }
    else
    {
        handle = connection->downloadFileChunk(peerId, fileName, chunkIndex, handleReply);
    }

    if (handle < 0)
        return {};

    return std::make_unique<BaseRequestContext<nx::Buffer>>(
        promise->get_future(), makeCanceller(promise, connection, handle));
}

QnMediaServerResourcePtr ResourcePoolPeerManager::getServer(const QnUuid& peerId) const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(peerId);
}

rest::ServerConnectionPtr ResourcePoolPeerManager::getConnection(const QnUuid& peerId) const
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
    SystemContext* systemContext,
    const PeerSelector& peerSelector)
    :
    ResourcePoolPeerManager(Capabilities(FileInfo | DownloadChunk), systemContext, peerSelector)
{
    setIndirectInternetRequestsAllowed(true);
}

} // namespace nx::vms::common::p2p::downloader
