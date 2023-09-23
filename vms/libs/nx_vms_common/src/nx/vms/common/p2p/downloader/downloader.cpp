// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "downloader.h"

#include <QtCore/QHash>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/delayed.h>

#include "private/internet_only_peer_manager.h"
#include "private/resource_pool_peer_manager.h"
#include "private/storage.h"
#include "private/worker.h"

namespace nx::vms::common::p2p::downloader {

class Downloader::Private: public QObject
{
    Downloader* const q = nullptr;

public:
    Private(Downloader* q);

    void startDownload(const QString& fileName);
    void stopDownload(const QString& fileName, bool emitSignals = true);

public:
    nx::Mutex mutex;
    QScopedPointer<Storage> storage;
    QHash<QString, std::shared_ptr<Worker>> workers;
    QList<AbstractPeerManager*> peerManagers;
    bool downloadsStarted = false;
};

Downloader::Private::Private(Downloader* q):
    q(q)
{
}

void Downloader::Private::startDownload(const QString& fileName)
{
    NX_MUTEX_LOCKER lock(&mutex);

    if (workers.contains(fileName))
        return;

    NX_INFO(this, "Starting download for %1", fileName);

    NX_ASSERT(storage->fileInformation(fileName).isValid());
    const auto status = storage->fileInformation(fileName).status;
    if (status == FileInformation::Status::downloaded)
    {
        emit q->downloadFinished(fileName);
        return;
    }

    if (status == FileInformation::Status::uploading)
        return;

    if (!downloadsStarted)
        return;

    const auto fi = storage->fileInformation(fileName);
    auto worker = std::make_shared<Worker>(
        fileName,
        storage.data(),
        peerManagers,
        q->peerId());
    workers[fileName] = worker;

    connect(worker.get(), &Worker::finished, this,
        [this](const QString& fileName) { stopDownload(fileName, true); });
    connect(worker.get(), &Worker::stalledChanged, q,
        [this, fileName](bool stalled) { emit q->downloadStalledChanged(fileName, stalled); });
    connect(worker.get(), &Worker::chunkDownloadFailed, q, &Downloader::chunkDownloadFailed);

    worker->start();
}

void Downloader::Private::stopDownload(const QString& fileName, bool emitSignals)
{
    NX_INFO(this, "Stopping download for %1", fileName);

    Worker::State state;
    {
        NX_MUTEX_LOCKER lock(&mutex);
        const auto worker = workers.take(fileName);
        if (!worker)
            return;

        state = worker->state();
        worker->stop();
    }

    if (emitSignals)
    {
        if (state == Worker::State::finished)
            emit q->downloadFinished(fileName);
        else
            emit q->downloadFailed(fileName);
    }
}

//-------------------------------------------------------------------------------------------------

Downloader::Downloader(
    const QDir& downloadsDirectory,
    SystemContext* systemContext,
    const QList<AbstractPeerManager*>& peerManagers,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
    NX_DEBUG(this, "Created");

    d->storage.reset(new Storage(downloadsDirectory));

    connect(d->storage.data(), &Storage::fileAdded, this,
        [this](const FileInformation& fileInformation)
        {
            emit fileAdded(fileInformation);
            d->startDownload(fileInformation.name);
        });

    connect(d->storage.data(), &Storage::fileDeleted, this,
        [this](const QString& fileName)
        {
            d->stopDownload(fileName, false);
            emit fileDeleted(fileName);
        });

    connect(d->storage.data(), &Storage::fileInformationChanged, this,
        &Downloader::fileInformationChanged);
    connect(d->storage.data(), &Storage::fileStatusChanged, this, &Downloader::fileStatusChanged);
    connect(d->storage.data(), &Storage::existingDownloadsLoaded, this,
        &Downloader::existingDownloadsLoaded);

    d->peerManagers = peerManagers;
    if (peerManagers.isEmpty())
    {
        d->peerManagers.append(new ResourcePoolPeerManager(systemContext));
        d->peerManagers.append(new InternetOnlyPeerManager());
        d->peerManagers.append(new ResourcePoolProxyPeerManager(systemContext));
    }
}

Downloader::~Downloader()
{
    stopDownloads();
    qDeleteAll(d->peerManagers);

    NX_DEBUG(this, "Deleted");
}

QStringList Downloader::files(const QString& type) const
{
    QStringList result;
    for (const auto& file: d->storage->files())
    {
        const auto fileInformation = this->fileInformation(file);
        if (fileInformation.isValid()
            && (type.isNull() || fileInformation.userData == type))
        {
            result.append(file);
        }
    }

    return result;
}

QString Downloader::filePath(const QString& fileName) const
{
    return d->storage->filePath(fileName);
}

QDir Downloader::downloadsDirectory() const
{
    return d->storage->downloadsDirectory();
}

FileInformation Downloader::fileInformation(const QString& fileName) const
{
    return d->storage->fileInformation(fileName);
}

ResultCode Downloader::addFile(const FileInformation& fileInformation)
{
    const auto result = d->storage->addFile(fileInformation);
    if (result == ResultCode::fileAlreadyExists)
        d->startDownload(fileInformation.name);
    return result;
}

ResultCode Downloader::moveFile(
    const QString& fileName, const QString& folderPath, const QString& type)
{
    return d->storage->moveFile(fileName, folderPath, type);
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
    d->stopDownload(fileName, false);
    return d->storage->deleteFile(fileName, deleteData);
}

ResultCode Downloader::deleteFilesByType(const QString& type)
{
    for (const auto& file: files(type))
    {
        d->stopDownload(file, false);
        const auto deleteResult = deleteFile(file, /*deleteData*/ true);
        if (deleteResult != ResultCode::ok && deleteResult != ResultCode::fileDoesNotExist)
        {
            NX_ERROR(this, "Failed to delete file '%1', Error: %2", file, deleteResult);
            return deleteResult;
        }

        NX_DEBUG(this, "File '%1' was successfully deleted", file);
    }

    return ResultCode::ok;
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
    NX_DEBUG(this, "Stopping downloads...");

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

void Downloader::loadExistingDownloads(bool waitForFinished)
{
    d->storage->loadExistingDownloads(waitForFinished);
}

bool Downloader::isStalled(const QString& fileName) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    if (const auto& worker = d->workers.value(fileName))
        return worker->isStalled();

    return false;
}

} // namespace nx::vms::common::p2p::downloader
