#include "internet_only_peer_manager.h"

#include <utils/common/delayed.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>

#include "../downloader.h"

using namespace std::chrono;

namespace nx::vms::common::p2p::downloader {

InternetOnlyPeerManager::InternetOnlyPeerManager():
    AbstractPeerManager(Capabilities(FileInfo | DownloadChunk))
{
}

InternetOnlyPeerManager::~InternetOnlyPeerManager()
{
}

QString InternetOnlyPeerManager::peerString(const QnUuid& peerId) const
{
    return peerId.isNull() ? QStringLiteral("Internet") : peerId.toString();
}

QList<QnUuid> InternetOnlyPeerManager::getAllPeers() const
{
    return {QnUuid()};
}

QList<QnUuid> InternetOnlyPeerManager::peers() const
{
    return {QnUuid()};
}

int InternetOnlyPeerManager::distanceTo(const QnUuid& /*peerId*/) const
{
    return 0;
}

AbstractPeerManager::RequestContext<FileInformation> InternetOnlyPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url)
{
    if (!peerId.isNull())
        return {};

    std::promise<std::optional<FileInformation>> promise;
    if (url.isValid())
        promise.set_value(FileInformation(fileName));
    else
        promise.set_value({});
    return {promise.get_future()};
}

AbstractPeerManager::RequestContext<QVector<QByteArray>> InternetOnlyPeerManager::requestChecksums(
    const QnUuid& /*peerId*/, const QString& /*fileName*/)
{
    return {};
}

AbstractPeerManager::RequestContext<QByteArray> InternetOnlyPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& /*fileName*/,
    const utils::Url& url,
    int chunkIndex,
    int chunkSize)
{
    constexpr milliseconds kDownloadRequestTimeout = 10min;

    if (!peerId.isNull())
        return {};

    auto httpClient = std::make_shared<nx::network::http::AsyncClient>();
    httpClient->bindToAioThread(m_aioTimer.getAioThread());
    httpClient->setResponseReadTimeout(kDownloadRequestTimeout);
    httpClient->setSendTimeout(kDownloadRequestTimeout);
    httpClient->setMessageBodyReadTimeout(kDownloadRequestTimeout);

    const qint64 pos = chunkIndex * chunkSize;
    httpClient->addAdditionalHeader("Range",
        QString("bytes=%1-%2").arg(pos).arg(pos + chunkSize - 1).toLatin1());

    auto promise = std::make_shared<std::promise<std::optional<QByteArray>>>();

    httpClient->doGet(url,
        [promise, httpClient, thread = thread()]()
        {
            if (httpClient->hasRequestSucceeded())
                setPromiseValueIfEmpty(promise, {httpClient->fetchMessageBodyBuffer()});
            else
                setPromiseValueIfEmpty(promise, {});

            httpClient->pleaseStopSync();
        });

    auto cancelRequest =
        [promise, httpClient]()
        {
            httpClient->pleaseStopSync();
            setPromiseValueIfEmpty(promise, {});
        };

    return {promise->get_future(), cancelRequest};
}

} // namespace nx::vms::common::p2p::downloader
