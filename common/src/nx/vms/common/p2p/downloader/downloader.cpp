#include "downloader.h"

#include <QtCore/QHash>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <utils/common/delayed.h>

#include "private/storage.h"
#include "private/worker.h"
#include "private/resource_pool_peer_manager.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

class DownloaderPrivate: public QObject
{
    Downloader* const q_ptr;
    Q_DECLARE_PUBLIC(Downloader)

public:
    DownloaderPrivate(Downloader* q);

    void createWorker(const QString& fileName);

private:
    void at_workerFinished(const QString& fileName);

private:
    QnMutex mutex;
    QScopedPointer<Storage> storage;
    QHash<QString, std::shared_ptr<Worker>> workers;
    AbstractPeerManagerFactory* peerManagerFactory = nullptr;
    std::unique_ptr<AbstractPeerManagerFactory> peerManagerFactoryOwner;
};

DownloaderPrivate::DownloaderPrivate(Downloader* q):
    QObject(q),
    q_ptr(q)
{
}

void DownloaderPrivate::createWorker(const QString& fileName)
{
    QnMutexLocker lock(&mutex);

    if (workers.contains(fileName))
        return;

    const auto status = storage->fileInformation(fileName).status;

    if (status != FileInformation::Status::downloaded
        && status != FileInformation::Status::uploading)
    {
        const auto fi = storage->fileInformation(fileName);
        auto worker = std::make_shared<Worker>(
            fileName,
            storage.data(),
            peerManagerFactory->createPeerManager(fi.peerPolicy, fi.additionalPeers));
        workers[fileName] = worker;

        connect(worker.get(), &Worker::finished, this, &DownloaderPrivate::at_workerFinished);
        connect(worker.get(), &Worker::failed, this, &DownloaderPrivate::at_workerFinished);
        connect(worker.get(), &Worker::chunkDownloadFailed, this->q_ptr, &Downloader::chunkDownloadFailed);

        worker->start();
    }
}

void DownloaderPrivate::at_workerFinished(const QString& fileName)
{
    Worker::State state;
    {
        QnMutexLocker lock(&mutex);
        auto worker = workers.take(fileName);
        if (!worker)
            return;

        state = worker->state();
        worker->stop();
    }

    Q_Q(Downloader);

    if (state == Worker::State::finished)
        emit q->downloadFinished(fileName);
    else
        emit q->downloadFailed(fileName);
}

//-------------------------------------------------------------------------------------------------

AbstractDownloader::AbstractDownloader(QObject* parent):
    QObject(parent)
{}

Downloader::Downloader(
    const QDir& downloadsDirectory,
    QnCommonModule* commonModule,
    AbstractPeerManagerFactory* peerManagerFactory,
    QObject* parent)
    :
    AbstractDownloader(parent),
    QnCommonModuleAware(commonModule),
    d_ptr(new DownloaderPrivate(this))
{
    Q_D(Downloader);
    d->storage.reset(new Storage(downloadsDirectory));

    connect(d->storage.data(), &Storage::fileAdded, this, &Downloader::fileAdded);
    connect(d->storage.data(), &Storage::fileDeleted, this, &Downloader::fileDeleted);
    connect(d->storage.data(), &Storage::fileInformationChanged, this,
        &Downloader::fileInformationChanged);
    connect(d->storage.data(), &Storage::fileStatusChanged, this, &Downloader::fileStatusChanged);

    d->peerManagerFactory = peerManagerFactory;
    if (!d->peerManagerFactory)
    {
        auto factory = std::make_unique<ResourcePoolPeerManagerFactory>(commonModule);
        d->peerManagerFactory = factory.get();
        d->peerManagerFactoryOwner = std::move(factory);
    }
}

void Downloader::atServerStart()
{
    Q_D(Downloader);
    for (const auto& fileName : d->storage->files())
        d->createWorker(fileName);
}

Downloader::~Downloader()
{
    Q_D(Downloader);
    for (auto& worker: d->workers)
        worker->stop();
}

QStringList Downloader::files() const
{
    Q_D(const Downloader);
    return d->storage->files();
}

QString Downloader::filePath(const QString& fileName) const
{
    Q_D(const Downloader);
    return d->storage->filePath(fileName);
}

FileInformation Downloader::fileInformation(
    const QString& fileName) const
{
    Q_D(const Downloader);
    return d->storage->fileInformation(fileName);
}

ResultCode Downloader::addFile(const FileInformation& fileInformation)
{
    Q_D(Downloader);

    auto errorCode = d->storage->addFile(fileInformation);
    if (errorCode != ResultCode::ok)
        return errorCode;

    executeInThread(thread(),
        [this, fileName = fileInformation.name]
        {
            Q_D(Downloader);
            d->createWorker(fileName);
        });

    return errorCode;
}

ResultCode Downloader::updateFileInformation(
    const QString& fileName,
    int size,
    const QByteArray& md5)
{
    Q_D(Downloader);
    return d->storage->updateFileInformation(fileName, size, md5);
}

ResultCode Downloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    Q_D(Downloader);
    return d->storage->readFileChunk(fileName, chunkIndex, buffer);
}

ResultCode Downloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& buffer)
{
    Q_D(Downloader);
    return d->storage->writeFileChunk(fileName, chunkIndex, buffer);
}

ResultCode Downloader::deleteFile(const QString& fileName, bool deleteData)
{
    Q_D(Downloader);
    {
        QnMutexLocker lock(&d->mutex);
        auto worker = d->workers.take(fileName);
        if (worker)
            worker->stop();
    }

    return d->storage->deleteFile(fileName, deleteData);
}

QVector<QByteArray> Downloader::getChunkChecksums(const QString& fileName)
{
    Q_D(Downloader);
    return d->storage->getChunkChecksums(fileName);
}

void Downloader::validateAsync(const QString& url, int expectedSize,
    std::function<void(bool)> callback)
{
    auto httpClient = createHttpClient();
    httpClient->doHead(url,
        [httpClient, url, callback, expectedSize](
            network::http::AsyncHttpClientPtr asyncClient) mutable
        {
            if (asyncClient->failed()
                || !asyncClient->response()
                || asyncClient->response()->statusLine.statusCode != network::http::StatusCode::ok)
            {
                auto response = asyncClient->response();
                NX_WARNING(
                    typeid(Downloader),
                    lm("[Downloader, validate] Validate %1 http request failed. "
                       "Http client failed: %2, has response: %3, status code: %4")
                        .args(url, asyncClient->failed(), (bool) response,
                            !response ? -1 : response->statusLine.statusCode));

                return callback(false);
            }

            auto& responseHeaders = asyncClient->response()->headers;
            auto contentLengthItr = responseHeaders.find("Content-Length");
            const bool hasHeader = contentLengthItr != responseHeaders.cend();

            if (!hasHeader || contentLengthItr->second.toInt() != expectedSize)
            {
                NX_WARNING(
                    typeid(Downloader),
                    lm("[Downloader, validate] %1. Content-Length: %2, fileInformation.size: %3")
                    .args(url,
                        hasHeader ? contentLengthItr->second.toInt() : -1,
                        expectedSize));

                return callback(false);
            }

            NX_VERBOSE(typeid(Downloader), lm("[Downloader, validate] %1. Success").args(url));
            callback(true);
        });
}

bool Downloader::validate(const QString& url, int expectedSize)
{
    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();

    validateAsync(url, expectedSize,
        [&readyPromise](bool success) mutable
        {
            readyPromise.set_value(success);
        });

    return readyFuture.get();
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
