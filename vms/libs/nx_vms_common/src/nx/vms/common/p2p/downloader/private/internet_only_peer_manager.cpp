// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

QString InternetOnlyPeerManager::peerString(const nx::Uuid& peerId) const
{
    return peerId.isNull() ? QStringLiteral("Internet") : peerId.toString();
}

QList<nx::Uuid> InternetOnlyPeerManager::getAllPeers() const
{
    return {nx::Uuid()};
}

QList<nx::Uuid> InternetOnlyPeerManager::peers() const
{
    return {nx::Uuid()};
}

int InternetOnlyPeerManager::distanceTo(const nx::Uuid& /*peerId*/) const
{
    return 0;
}

AbstractPeerManager::RequestContextPtr<FileInformation> InternetOnlyPeerManager::requestFileInfo(
    const nx::Uuid& peerId,
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

    return std::make_unique<InternetRequestContext<FileInformation>>(
        nullptr, promise.get_future());
}

AbstractPeerManager::RequestContextPtr<QVector<QByteArray>> InternetOnlyPeerManager::requestChecksums(
    const nx::Uuid& /*peerId*/, const QString& /*fileName*/)
{
    return std::make_unique<InternetRequestContext<QVector<QByteArray>>>();
}

AbstractPeerManager::RequestContextPtr<nx::Buffer> InternetOnlyPeerManager::downloadChunk(
    const nx::Uuid& peerId,
    const QString& /*fileName*/,
    const utils::Url& url,
    int chunkIndex,
    int chunkSize,
    qint64 fileSize)
{
    constexpr milliseconds kDownloadRequestTimeout = 1min;

    if (!peerId.isNull())
        return {};

    auto httpClient = std::make_unique<nx::network::http::AsyncClient>(
        nx::network::ssl::kDefaultCertificateCheck);
    httpClient->bindToAioThread(m_aioTimer.getAioThread());
    httpClient->setResponseReadTimeout(kDownloadRequestTimeout);
    httpClient->setSendTimeout(kDownloadRequestTimeout);
    httpClient->setMessageBodyReadTimeout(kDownloadRequestTimeout);

    const qint64 pos = chunkIndex * chunkSize;
    httpClient->addAdditionalHeader("Range",
        nx::format("bytes=%1-%2", pos, std::min(pos + chunkSize, fileSize) - 1).toStdString());

    auto promise = std::make_shared<std::promise<std::optional<nx::Buffer>>>();

    httpClient->doGet(url,
        [promise, httpClient = httpClient.get()]()
        {
            if (httpClient->hasRequestSucceeded())
                setPromiseValueIfEmpty(promise, {httpClient->fetchMessageBodyBuffer()});
            else
                setPromiseValueIfEmpty(promise, {});
        });

    std::function<void()> cancelRequest =
        [promise, httpClient = httpClient.get()]() { setPromiseValueIfEmpty(promise, {}); };

    return std::make_unique<InternetRequestContext<nx::Buffer>>(
        std::move(httpClient), promise->get_future(), cancelRequest);
}

} // namespace nx::vms::common::p2p::downloader
