#include "resource_pool_peer_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/network/http/async_http_client_reply.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

namespace {

static const int kDownloadRequestTimeoutMs = 10 * 60 * 1000;

} // namespace

ResourcePoolPeerManager::ResourcePoolPeerManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
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
        result = lit("%1 (%2 %3)").arg(
            server->getName(),
            server->getPrimaryAddress().toString(),
            server->getId().toString());
    }
    else
    {
        result = lit("Unknown server %1").arg(peerId.toString());
    }

    if (peerId == selfId())
        result += lit(" (self)");

    return result;
}

QList<QnUuid> ResourcePoolPeerManager::getAllPeers() const
{
    const auto& servers = resourcePool()->getAllServers(Qn::Online);
    const auto& currentId = selfId();

    QList<QnUuid> result;

    for (const auto& server: servers)
    {
        const auto& id = server->getId();
        if (id != currentId)
            result.append(id);
    }

    return result;
}

int ResourcePoolPeerManager::distanceTo(const QnUuid& peerId) const
{
    const auto& connection = commonModule()->ec2Connection();
    if (!connection)
        return -1;

    int distance = std::numeric_limits<int>::max();
    connection->routeToPeerVia(peerId, &distance);
    return distance;
}

bool ResourcePoolPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    const auto& server = getServer(peerId);
    NX_ASSERT(server);
    if (!server)
        return false;

    return server->getServerFlags().testFlag(Qn::SF_HasPublicIP);
}

rest::Handle ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId, const QString& fileName, FileInfoCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    auto handleReply =
        [this, callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (!success)
                callback(success, handle, FileInformation());

            const auto& fileInfo = result.deserialized<FileInformation>();
            callback(success, handle, fileInfo);
        };

    return connection->fileDownloadStatus(fileName, handleReply, thread());
}

rest::Handle ResourcePoolPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    auto handleReply =
        [this, callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (!success)
                callback(success, handle, QVector<QByteArray>());

            const auto& checksums = result.deserialized<QVector<QByteArray>>();
            callback(success, handle, checksums);
        };

    return connection->fileChunkChecksums(fileName, handleReply, thread());
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

    return connection->downloadFileChunk(fileName, chunkIndex, callback, thread());
}

rest::Handle ResourcePoolPeerManager::downloadChunkFromInternet(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    if (peerId != selfId())
    {
        const auto& connection = getConnection(peerId);
        if (!connection)
            return -1;

        auto handleReply =
            [this, callback](bool success, rest::Handle handle, const QByteArray& result)
            {
                if (!success)
                    callback(success, handle, QByteArray());

                callback(success, handle, result);
            };

        return connection->downloadFileChunkFromInternet(
            fileName, url, chunkIndex, chunkSize, handleReply, thread());
    }

    auto httpClient = nx::network::http::AsyncHttpClient::create();
    httpClient->setResponseReadTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setSendTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setMessageBodyReadTimeoutMs(kDownloadRequestTimeoutMs);

    const qint64 pos = chunkIndex * chunkSize;
    httpClient->addAdditionalHeader("Range",
        lit("bytes=%1-%2").arg(pos).arg(pos + chunkSize - 1).toLatin1());

    const auto handle = ++m_currentSelfRequestHandle;

    auto reply = new QnAsyncHttpClientReply(httpClient, this);
    m_replyByHandle[handle] = reply;

    connect(reply, &QnAsyncHttpClientReply::finished, this,
        [this, handle, callback](QnAsyncHttpClientReply* reply)
        {
            if (!m_replyByHandle.remove(handle))
                return;

            QByteArray result;

            if (!reply->isFailed())
                result = reply->data();

            callback(!result.isNull(), handle, result);
        });

    httpClient->doGet(url);

    return handle;
}

void ResourcePoolPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    if (peerId == selfId())
    {
        m_replyByHandle.remove(handle);
        return;
    }

    const auto& connection = getConnection(peerId);
    if (!connection)
        return;

    connection->cancelRequest(handle);
}

QnMediaServerResourcePtr ResourcePoolPeerManager::getServer(const QnUuid& peerId) const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(peerId);
}

rest::QnConnectionPtr ResourcePoolPeerManager::getConnection(const QnUuid& peerId) const
{
    NX_ASSERT(peerId != selfId());
    if (peerId == selfId())
        return rest::QnConnectionPtr();

    const auto& server = getServer(peerId);
    if (!server)
        return rest::QnConnectionPtr();

    return server->restConnection();
}

ResourcePoolPeerManagerFactory::ResourcePoolPeerManagerFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

AbstractPeerManager* ResourcePoolPeerManagerFactory::createPeerManager()
{
    return new ResourcePoolPeerManager(commonModule());
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
