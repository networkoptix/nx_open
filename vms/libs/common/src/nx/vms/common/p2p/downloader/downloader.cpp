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

namespace nx::vms::common::p2p::downloader {

class Downloader::Private: public QObject
{
    Downloader* const q = nullptr;

public:
    Private(Downloader* q);

    void startDownload(const QString& fileName);

private:
    void atWorkerFinished(const QString& fileName);

public:
    QnMutex mutex;
    QScopedPointer<Storage> storage;
    QHash<QString, std::shared_ptr<Worker>> workers;
    AbstractPeerManagerFactory* peerManagerFactory = nullptr;
    std::unique_ptr<AbstractPeerManagerFactory> peerManagerFactoryOwner;
    bool downloadsStarted = false;
};

Downloader::Private::Private(Downloader* q):
    q(q)
{
}

void Downloader::Private::startDownload(const QString& fileName)
{
    NX_MUTEX_LOCKER lock(&mutex);

    if (!downloadsStarted)
        return;

    const auto keyFileName = FileInformation::keyFromFileName(fileName);
    if (workers.contains(keyFileName))
        return;

    const auto status = storage->fileInformation(fileName).status;

    if (status != FileInformation::Status::downloaded
        && status != FileInformation::Status::uploading)
    {
        const auto fi = storage->fileInformation(fileName);
        auto peerManager = peerManagerFactory->createPeerManager(fi.peerPolicy, fi.additionalPeers);
        auto worker = std::make_shared<Worker>(
            storage->filePath(fileName),
            storage.data(),
            peerManager);
        workers[keyFileName] = worker;

        connect(worker.get(), &Worker::finished, this, &Downloader::Private::atWorkerFinished);
        connect(worker.get(), &Worker::failed, this, &Downloader::Private::atWorkerFinished);
        connect(worker.get(), &Worker::chunkDownloadFailed, q, &Downloader::chunkDownloadFailed);

        worker->start();
    }
}

void Downloader::Private::atWorkerFinished(const QString& fileName)
{
    Worker::State state;
    {
        NX_MUTEX_LOCKER lock(&mutex);
        const auto worker = workers.take(FileInformation::keyFromFileName(fileName));
        if (!worker)
            return;

        state = worker->state();
        worker->stop();
    }

    if (state == Worker::State::finished)
        emit q->downloadFinished(fileName);
    else
        emit q->downloadFailed(fileName);
}

//-------------------------------------------------------------------------------------------------

Downloader::Downloader(
    const QDir& downloadsDirectory,
    QnCommonModule* commonModule,
    AbstractPeerManagerFactory* peerManagerFactory,
    QObject* parent)
    :
    QObject(parent),
    QnCommonModuleAware(commonModule),
    d(new Private(this))
{
    d->storage.reset(new Storage(downloadsDirectory));

    connect(d->storage.data(), &Storage::fileAdded, this, &Downloader::fileAdded);
    connect(d->storage.data(), &Storage::fileDeleted, this, &Downloader::fileDeleted);
    connect(d->storage.data(), &Storage::fileInformationChanged, this,
        &Downloader::fileInformationChanged);
    connect(d->storage.data(), &Storage::fileStatusChanged, this, &Downloader::fileStatusChanged);

    d->peerManagerFactory = peerManagerFactory;

    // Creating the default factory.
    if (!d->peerManagerFactory)
    {
        auto factory = std::make_unique<ResourcePoolPeerManagerFactory>(commonModule);
        d->peerManagerFactory = factory.get();
        d->peerManagerFactoryOwner = std::move(factory);
    }

    QMetaObject::invokeMethod(
        d->storage.data(),
        [this]() { d->storage->findDownloads(); },
        Qt::QueuedConnection);
}

Downloader::~Downloader()
{
    for (auto& worker: d->workers)
        worker->stop();
}

QStringList Downloader::files() const
{
    return d->storage->files();
}

QString Downloader::filePath(const QString& fileName) const
{
    return d->storage->filePath(fileName);
}

FileInformation Downloader::fileInformation(const QString& fileName) const
{
    return d->storage->fileInformation(fileName);
}

ResultCode Downloader::addFile(const FileInformation& fileInformation)
{
    auto errorCode = d->storage->addFile(fileInformation);
    if (errorCode != ResultCode::ok)
        return errorCode;

    executeInThread(thread(),
        [this, fileName = fileInformation.name]
        {
            d->startDownload(fileName);
        });

    return errorCode;
}

ResultCode Downloader::updateFileInformation(
    const QString& fileName,
    int size,
    const QByteArray& md5)
{
    return d->storage->updateFileInformation(fileName, size, md5);
}

ResultCode Downloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    return d->storage->readFileChunk(fileName, chunkIndex, buffer);
}

ResultCode Downloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& buffer)
{
    return d->storage->writeFileChunk(fileName, chunkIndex, buffer);
}

ResultCode Downloader::deleteFile(const QString& fileName, bool deleteData)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        auto worker = d->workers.take(FileInformation::keyFromFileName(fileName));
        if (worker)
            worker->stop();
    }

    return d->storage->deleteFile(fileName, deleteData);
}

QVector<QByteArray> Downloader::getChunkChecksums(const QString& fileName)
{
    return d->storage->getChunkChecksums(fileName);
}

void Downloader::startDownloads()
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->downloadsStarted = true;
    }

    for (const auto& fileName: d->storage->files())
        d->startDownload(fileName);
}

void Downloader::stopDownloads()
{
    decltype(d->workers) workers;

    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->downloadsStarted = false;

        workers = d->workers;
        d->workers.clear();
    }

    for (const auto& worker: workers)
        worker->stop();
}

void Downloader::validateAsync(const QString& url, bool onlyConnectionCheck, int expectedSize,
    std::function<void(bool)> callback)
{
    auto httpClient = createHttpClient();
    httpClient->doHead(url,
        [httpClient, url, callback, expectedSize, onlyConnectionCheck](
            const network::http::AsyncHttpClientPtr& asyncClient) mutable
        {
            const auto& response = asyncClient->response();
            const bool hasResponse = response != nullptr;

            if (asyncClient->failed()
                || !hasResponse
                || response->statusLine.statusCode != network::http::StatusCode::ok)
            {
                NX_WARNING(NX_SCOPE_TAG,
                    "validateAsync(): Validate %1 http request failed. "
                       "Http client failed: %2, has response: %3, status code: %4",
                    url,
                    asyncClient->failed(),
                    hasResponse,
                    response ? response->statusLine.statusCode : -1);

                return callback(false);
            }

            if (onlyConnectionCheck)
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "validateAsync(): %1. Success (only connection check)", url);
                return callback(true);
            }

            auto& responseHeaders = asyncClient->response()->headers;
            auto contentLengthItr = responseHeaders.find("Content-Length");
            const bool hasHeader = contentLengthItr != responseHeaders.cend();

            if (!hasHeader || contentLengthItr->second.toInt() != expectedSize)
            {
                NX_WARNING(NX_SCOPE_TAG,
                    "validateAsync(): %1. Content-Length: %2, fileInformation.size: %3",
                    url,
                    hasHeader ? contentLengthItr->second.toInt() : -1,
                    expectedSize);

                return callback(false);
            }

            NX_VERBOSE(NX_SCOPE_TAG, "validateAsync(): %1. Success", url);
            callback(true);
        });
}

bool Downloader::validate(const QString& url, bool onlyConnectionCheck, int expectedSize)
{
    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();

    validateAsync(url, onlyConnectionCheck, expectedSize,
        [&readyPromise](bool success) mutable
        {
            readyPromise.set_value(success);
        });

    return readyFuture.get();
}

} // namespace nx::vms::common::p2p::downloader
