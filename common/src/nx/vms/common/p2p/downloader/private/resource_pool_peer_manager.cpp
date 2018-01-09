#include "resource_pool_peer_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/network/http/async_http_client_reply.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>
#include "peer_selection/peer_selector_factory.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

namespace {

static const int kDownloadRequestTimeoutMs = 10 * 60 * 1000;
using namespace peer_selection;

class OtherPeerInfosProvider
{
public:
    explicit OtherPeerInfosProvider(QnCommonModule* commonModule)
    {
        const auto selfId = commonModule->moduleGUID();
        const auto servers = commonModule->resourcePool()->getAllServers(Qn::Online);
        for (const auto& server : servers)
            addInfo(server, selfId);
    }

    QList<QnUuid> ids() const
    {
        QList<QnUuid> result;
        for (const auto& peerInfo: m_peerInfos)
            result.append(peerInfo.id);
        return result;
    }

    PeerInformationList peerInfos() const
    {
        return m_peerInfos;
    }
private:
    PeerInformationList m_peerInfos;

    void addInfo(const QnMediaServerResourcePtr& server, const QnUuid& selfId)
    {
        if (server->getId() == selfId)
            return;
        m_peerInfos.append(PeerInformation(server->getSystemInfo(), server->getId()));
    }
};

} // namespace

ResourcePoolPeerManager::ResourcePoolPeerManager(
    QnCommonModule* commonModule,
    peer_selection::AbstractPeerSelectorPtr peerSelector)
    :
    QnCommonModuleAware(commonModule),
    m_peerSelector(std::move(peerSelector))
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
    return OtherPeerInfosProvider(commonModule()).ids();
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

QList<QnUuid> ResourcePoolPeerManager::peers() const
{
    const auto allOtherPeerInfos = OtherPeerInfosProvider(commonModule()).peerInfos();
    return m_peerSelector->peers(allOtherPeerInfos);
}

ResourcePoolPeerManagerFactory::ResourcePoolPeerManagerFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

AbstractPeerManager* ResourcePoolPeerManagerFactory::createPeerManager(
    FileInformation::PeerPolicy peerPolicy)
{
    return new ResourcePoolPeerManager(
        commonModule(),
        PeerSelectorFactory::create(peerPolicy, commonModule()));
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
