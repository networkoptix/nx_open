#include "internet_only_peer_manager.h"

#include <memory>

#include <utils/common/delayed.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>

#include "../downloader.h"

using namespace std::chrono;

namespace nx::vms::common::p2p::downloader {

namespace {

static const milliseconds kDownloadRequestTimeout = 10min;

} // namespace

class InternetOnlyPeerManager::Private
{
public:
    QnMutex mutex;
    rest::Handle nextRequestId = 1;
    QHash<rest::Handle, std::shared_ptr<nx::network::http::AsyncClient>> requestClients;
};

InternetOnlyPeerManager::InternetOnlyPeerManager():
    d(new Private())
{
}

InternetOnlyPeerManager::~InternetOnlyPeerManager()
{
}

QnUuid InternetOnlyPeerManager::selfId() const
{
    return {};
}

QString InternetOnlyPeerManager::peerString(const QnUuid& peerId) const
{
    return peerId.isNull() ? QStringLiteral("Internet") : peerId.toString();
}

QList<QnUuid> InternetOnlyPeerManager::getAllPeers() const
{
    return {};
}

QList<QnUuid> InternetOnlyPeerManager::peers() const
{
    return {};
}

int InternetOnlyPeerManager::distanceTo(const QnUuid& /*peerId*/) const
{
    return 0;
}

bool InternetOnlyPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    // TODO: Actually check Internet connection.
    return peerId.isNull();
}

rest::Handle InternetOnlyPeerManager::requestFileInfo(
    const QnUuid& /*peerId*/,
    const QString& /*fileName*/,
    AbstractPeerManager::FileInfoCallback /*callback*/)
{
    return -1;
}

rest::Handle InternetOnlyPeerManager::requestChecksums(
    const QnUuid& /*peerId*/,
    const QString& /*fileName*/,
    AbstractPeerManager::ChecksumsCallback /*callback*/)
{
    return -1;
}

rest::Handle InternetOnlyPeerManager::downloadChunk(
    const QnUuid& /*peerId*/,
    const QString& /*fileName*/,
    int /*chunkIndex*/,
    AbstractPeerManager::ChunkCallback /*callback*/)
{
    return -1;
}

rest::Handle InternetOnlyPeerManager::downloadChunkFromInternet(
    const QnUuid& peerId,
    const QString& /*fileName*/,
    const utils::Url& url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    if (!peerId.isNull())
        return -1;

    auto httpClient = std::make_shared<nx::network::http::AsyncClient>();
    httpClient->bindToAioThread(m_aioTimer.getAioThread());
    httpClient->setResponseReadTimeout(kDownloadRequestTimeout);
    httpClient->setSendTimeout(kDownloadRequestTimeout);
    httpClient->setMessageBodyReadTimeout(kDownloadRequestTimeout);

    const qint64 pos = chunkIndex * chunkSize;
    httpClient->addAdditionalHeader("Range",
        QString("bytes=%1-%2").arg(pos).arg(pos + chunkSize - 1).toLatin1());

    const auto requestId = d->nextRequestId;

    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->requestClients[requestId] = httpClient;
    }

    ++d->nextRequestId;

    httpClient->doGet(url,
        nx::utils::guarded(this,
            [this, callback, requestId, thread = thread()]()
            {
                executeInThread(thread, nx::utils::guarded(this,
                    [this, callback, requestId]()
                    {
                        NX_MUTEX_LOCKER lock(&d->mutex);
                        const auto httpClient = d->requestClients.take(requestId);
                        lock.unlock();

                        if (!httpClient)
                            return;

                        const bool success = httpClient->hasRequestSucceeded();

                        QByteArray result;
                        if (success)
                            result = httpClient->fetchMessageBodyBuffer();

                        callback(success, requestId, result);
                    }));
            }));

    return requestId;
}

void InternetOnlyPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    if (!peerId.isNull())
        return;

    NX_MUTEX_LOCKER lock(&d->mutex);
    d->requestClients.remove(handle);
}

bool InternetOnlyPeerManager::hasAccessToTheUrl(const QString& url) const
{
    if (url.isEmpty())
        return false;

    return Downloader::validate(url, /* onlyConnectionCheck */ true, /* expectedSize */ 0);
}

AbstractPeerManager* InternetOnlyPeerManagerFactory::createPeerManager(
    FileInformation::PeerSelectionPolicy /*peerPolicy*/,
    const QList<QnUuid>& /*additionalPeers*/)
{
    return new InternetOnlyPeerManager();
}

} // namespace nx::vms::common::p2p::downloader
