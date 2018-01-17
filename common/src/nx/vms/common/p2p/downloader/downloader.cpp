#include "downloader.h"

#include <QtCore/QHash>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
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
    QHash<QString, Worker*> workers;
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

    NX_ASSERT(!workers.contains(fileName));
    if (workers.contains(fileName))
        return;

    const auto status = storage->fileInformation(fileName).status;

    if (status != FileInformation::Status::downloaded
        && status != FileInformation::Status::uploading)
    {
        auto peerPolicy = storage->fileInformation(fileName).peerPolicy;
        auto worker = new Worker(
            fileName,
            storage.data(),
            peerManagerFactory->createPeerManager(peerPolicy));
        workers[fileName] = worker;

        connect(worker, &Worker::finished, this, &DownloaderPrivate::at_workerFinished);
        connect(worker, &Worker::failed, this, &DownloaderPrivate::at_workerFinished);

        worker->start();
    }
}

void DownloaderPrivate::at_workerFinished(const QString& fileName)
{
    auto worker = workers.take(fileName);
    if (!worker)
        return;

    Q_Q(Downloader);

    const auto state = worker->state();
    if (state == Worker::State::finished)
        emit q->downloadFinished(fileName);
    else
        emit q->downloadFailed(fileName);

    delete worker;
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
    qDeleteAll(d->workers);
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

ResultCode Downloader::deleteFile(
    const QString& fileName,
    bool deleteData)
{
    Q_D(Downloader);
    return d->storage->deleteFile(fileName, deleteData);
}

QVector<QByteArray> Downloader::getChunkChecksums(const QString& fileName)
{
    Q_D(Downloader);
    return d->storage->getChunkChecksums(fileName);
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
