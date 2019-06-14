#include "resource_pool_peer_manager.h"

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

namespace {

template<typename Promise>
auto makeCanceller(Promise promise, const rest::QnConnectionPtr& connection, rest::Handle handle)
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
    QHash<QnUuid, rest::QnConnectionPtr> directConnectionByServerId;
    PeerSelector peerSelector;

    QnMutex mutex;
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

AbstractPeerManager::RequestContext<FileInformation> ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<FileInformation>>>();

    if (d->allowIndirectInternetRequests)
    {
        const auto& server = resourcePool()->getResourceById<QnMediaServerResource>(peerId);
        const bool hasInternet =
            !server || server->getServerFlags().testFlag(vms::api::SF_HasPublicIP);

        if (hasInternet && url.isValid())
            setPromiseValueIfEmpty(promise, {FileInformation(fileName)});
        else
            setPromiseValueIfEmpty(promise, {});

        return {promise->get_future()};
    }

    auto handleReply =
        [promise](bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            if (success)
                setPromiseValueIfEmpty(promise, {result.deserialized<FileInformation>()});
            else
                setPromiseValueIfEmpty(promise, {});
        };

    const auto handle = connection->fileDownloadStatus(fileName, std::move(handleReply), thread());
    if (handle < 0)
        return {};

    return {promise->get_future(), makeCanceller(promise, connection, handle)};
}

AbstractPeerManager::RequestContext<QVector<QByteArray>> ResourcePoolPeerManager::requestChecksums(
    const QnUuid& peerId, const QString& fileName)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<QVector<QByteArray>>>>();

    const auto handleReply =
        [promise](bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            if (success)
                setPromiseValueIfEmpty(promise, {result.deserialized<QVector<QByteArray>>()});
            else
                setPromiseValueIfEmpty(promise, {});
        };

    const auto handle = connection->fileChunkChecksums(fileName, handleReply, thread());
    if (handle < 0)
        return {};

    return {promise->get_future(), makeCanceller(promise, connection, handle)};
}

AbstractPeerManager::RequestContext<QByteArray> ResourcePoolPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return {};

    auto promise = std::make_shared<std::promise<std::optional<QByteArray>>>();

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
            fileName, url, chunkIndex, chunkSize, handleReply, thread());
    }
    else
    {
        handle = connection->downloadFileChunk(fileName, chunkIndex, handleReply, thread());
    }

    if (handle < 0)
        return {};

    return {promise->get_future(), makeCanceller(promise, connection, handle)};
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
