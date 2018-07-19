#include <nx/utils/log/assert.h>
#include <nx/network/http/async_http_client_reply.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>
#include "peer_selection/peer_selector_factory.h"
#include "../validate_result.h"
#include "../downloader.h"
#include "resource_pool_peer_manager.h"

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
        for (const auto& server: servers)
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

nx::network::http::AsyncHttpClientPtr createHttpClient()
{
    auto httpClient = nx::network::http::AsyncHttpClient::create();
    httpClient->setResponseReadTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setSendTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient->setMessageBodyReadTimeoutMs(5);

    return httpClient;
}

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
    connection->routeToPeerVia(peerId, &distance, /*address*/ nullptr);
    return distance;
}

bool ResourcePoolPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    const auto& server = getServer(peerId);
    NX_ASSERT(server);
    if (!server)
        return false;

    return server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
}

bool ResourcePoolPeerManager::hasAccessToTheUrl(const QString& url) const
{
    if (url.isEmpty())
        return false;

    return Downloader::validate(url, /* onlyConnectionCheck */ true, /* expectedSize */ 0);
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
                return callback(success, handle, FileInformation());

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
                return callback(success, handle, QVector<QByteArray>());

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

    const auto internalCallback =
        [callback = std::move(callback)](
            bool success,
            rest::Handle requestId,
            QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            return callback(success, requestId, result);
        };

    return connection->downloadFileChunk(fileName, chunkIndex, std::move(internalCallback),
        thread());
}

rest::Handle ResourcePoolPeerManager::validateFileInformation(
    const QnUuid& peerId,
    const FileInformation& fileInformation,
    AbstractPeerManager::ValidateCallback callback)
{
    if (peerId != selfId())
    {
        NX_VERBOSE(
            this,
            lm("[Downloader, validate] File %1 via other peer %2")
                .args(fileInformation.name, peerId));

        const auto& connection = getConnection(peerId);
        if (!connection)
        {
            NX_WARNING(this, lm("[Downloader, validate] File %1. No rest connection for %2")
                .args(fileInformation.name, peerId));
            return -1;
        }

        auto handleReply =
            [this, callback, fileInformation, peerId](
                bool success,
                rest::Handle handle,
                const QnJsonRestResult& result)
            {
                if (!success)
                {
                    NX_WARNING(this, lm("[Downloader, validate] File %1. Rest connection to %2 response error")
                        .args(fileInformation.name, peerId));
                    return callback(success, handle);
                }

                const auto validateResult = result.deserialized<ValidateResult>();
                if (!validateResult.success)
                {
                    NX_WARNING(this, lm("[Downloader, validate] File %1. peer %2 responded that validation had failed")
                        .args(fileInformation.name, peerId));
                    return callback(false, handle);
                }

                NX_VERBOSE(this, lm("[Downloader, validate] File %1. peer %2 validation successful")
                    .args(fileInformation.name, peerId));

                callback(success, handle);
            };

        return connection->validateFileInformation(
            QString::fromLatin1(fileInformation.url.toString().toLocal8Bit().toBase64()),
            fileInformation.size, handleReply, thread());
    }

    NX_VERBOSE(
        this,
        lm("[Downloader, validate] Trying to validate %1 directly")
            .args(fileInformation.name));

    auto handle = ++m_currentSelfRequestHandle;
    if (handle < 0)
    {
        m_currentSelfRequestHandle = 1;
        handle = 1;
    }

    Downloader::validateAsync(
        fileInformation.url.toString(), /* onlyConnectionCheck */ false, fileInformation.size,
        [this, callback, handle](bool success)
        {
            callback(success, handle);
        });

    return handle;
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

        const auto handleReply =
            [this, callback = std::move(callback)](
                bool success,
                rest::Handle requestId,
                QByteArray result,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                if (!success)
                    return callback(success, requestId, QByteArray());

                // TODO: #vkutin #common Is double call intended?
                return callback(success, requestId, result);
            };

        return connection->downloadFileChunkFromInternet(
            fileName, url, chunkIndex, chunkSize, std::move(handleReply), thread());
    }

    const qint64 pos = chunkIndex * chunkSize;
    auto httpClient = createHttpClient();
    httpClient->addAdditionalHeader("Range",
        lit("bytes=%1-%2").arg(pos).arg(pos + chunkSize - 1).toLatin1());

    auto handle = ++m_currentSelfRequestHandle;
    if (handle < 0)
    {
        m_currentSelfRequestHandle = 1;
        handle = 1;
    }

    m_httpClientByHandle[handle] = httpClient;

    qWarning() << "Starting http client" << url.toString() << handle;
    httpClient->doGet(url,
        [this, handle, callback, httpClient, url](network::http::AsyncHttpClientPtr client)
        {
            if (!m_httpClientByHandle.remove(handle))
                return;
            qWarning() << "http client done" << url.toString() << handle;
            using namespace network;
            QByteArray result;

            auto okResponse = client->response() &&
                (client->response()->statusLine.statusCode == http::StatusCode::partialContent
                || client->response()->statusLine.statusCode == http::StatusCode::ok);

            if (!client->failed() && okResponse)
                result = client->fetchMessageBodyBuffer();

            callback(!result.isNull(), handle, result);
        });

    return handle;
}

void ResourcePoolPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    if (peerId == selfId())
    {
        m_httpClientByHandle.remove(handle);
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
    FileInformation::PeerSelectionPolicy peerPolicy,
    const QList<QnUuid>& additionalPeers)
{
    return new ResourcePoolPeerManager(
        commonModule(),
        PeerSelectorFactory::create(peerPolicy, additionalPeers, commonModule()));
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
