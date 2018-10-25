#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <nx/vms/common/p2p/downloader/validate_result.h>
#include <nx_ec/ec_api.h>
#include "single_connection_peer_manager.h"


namespace nx::vms::common::p2p::downloader {

SingleConnectionPeerManager::SingleConnectionPeerManager(
    QnCommonModule* commonModule,
    AbstractPeerSelectorPtr&& peerSelector)
    :
    QnCommonModuleAware(commonModule),
    m_peerSelector(std::move(peerSelector))
{
}

QnUuid SingleConnectionPeerManager::selfId() const
{
    return commonModule()->moduleGUID();
}

QString SingleConnectionPeerManager::peerString(const QnUuid& peerId) const
{
    QString result = peerId.toString();

    if (peerId == selfId())
        result += "(self)";

    return result;
}

QList<QnUuid> SingleConnectionPeerManager::getAllPeers() const
{
    return {selfId(), getServerId()};
}

int SingleConnectionPeerManager::distanceTo(const QnUuid& peerId) const
{
    const auto& connection = commonModule()->ec2Connection();
    if (!connection)
        return -1;

    int distance = std::numeric_limits<int>::max();
    connection->routeToPeerVia(peerId, &distance, /*address*/ nullptr);
    return distance;
}

bool SingleConnectionPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    // TODO: Is it really so true?
    if (peerId == selfId())
    {
        // TODO: Ask client if it has internet.
        return true;
    }
    else
    {
        // TODO: Ask server if it has internet.
    }
    return true;
}

bool SingleConnectionPeerManager::hasAccessToTheUrl(const QString& url) const
{
    if (url.isEmpty())
        return false;

    return Downloader::validate(url, /* onlyConnectionCheck */ true, /* expectedSize */ 0);
}

rest::Handle SingleConnectionPeerManager::requestFileInfo(
    const QnUuid& peerId, const QString& fileName, FileInfoCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    auto handleReply =
        [callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (!success)
                return callback(success, handle, FileInformation());

            const auto& fileInfo = result.deserialized<FileInformation>();
            callback(success, handle, fileInfo);
        };

    return connection->fileDownloadStatus(fileName, handleReply, thread());
}

rest::Handle SingleConnectionPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    auto handleReply =
        [callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (!success)
                return callback(success, handle, QVector<QByteArray>());

            const auto& checksums = result.deserialized<QVector<QByteArray>>();
            callback(success, handle, checksums);
        };

    return connection->fileChunkChecksums(fileName, handleReply, thread());
}

rest::Handle SingleConnectionPeerManager::downloadChunk(
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

rest::Handle SingleConnectionPeerManager::validateFileInformation(
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
            [callback, fileInformation, peerId](
                bool success,
                rest::Handle handle,
                const QnJsonRestResult& result)
            {
                if (!success)
                {
                    NX_WARNING(typeid(AbstractPeerManager), lm("[Downloader, validate] File %1. Rest connection to %2 response error")
                        .args(fileInformation.name, peerId));
                    return callback(success, handle);
                }

                const auto validateResult = result.deserialized<ValidateResult>();
                if (!validateResult.success)
                {
                    NX_WARNING(typeid(AbstractPeerManager), lm("[Downloader, validate] File %1. peer %2 responded that validation had failed")
                        .args(fileInformation.name, peerId));
                    return callback(false, handle);
                }

                NX_VERBOSE(typeid(AbstractPeerManager), lm("[Downloader, validate] File %1. peer %2 validation successful")
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
        [callback, handle](bool success)
        {
            callback(success, handle);
        });

    return handle;
}

rest::Handle SingleConnectionPeerManager::downloadChunkFromInternet(
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
            [callback = std::move(callback)](
                bool success,
                rest::Handle requestId,
                QByteArray result,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                return callback(success, requestId, success ? result : QByteArray());
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

    httpClient->doGet(url,
        [this, handle, callback, httpClient, url](network::http::AsyncHttpClientPtr client)
        {
            if (!m_httpClientByHandle.remove(handle))
                return;
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

void SingleConnectionPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
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

void SingleConnectionPeerManager::setConnection(rest::QnConnectionPtr connection)
{
    m_directConnection = connection;
}

rest::QnConnectionPtr SingleConnectionPeerManager::getConnection(const QnUuid& peerId) const
{
    NX_ASSERT(peerId != selfId());
    if (peerId == selfId())
        return rest::QnConnectionPtr();
    NX_ASSERT(m_directConnection);
    return m_directConnection;
}

QnUuid SingleConnectionPeerManager::getServerId() const
{
    if (m_directConnection)
        return m_directConnection->getServerId();
    return QnUuid();
}


QList<QnUuid> SingleConnectionPeerManager::peers() const
{
    return {getServerId()};
}

} // namespace nx::vms::common::p2p::downloader
