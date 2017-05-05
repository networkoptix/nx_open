#include "distributed_file_downloader.h"

#include <QtCore/QHash>

#include <nx/utils/thread/mutex.h>
#include <utils/common/delayed.h>
#include <nx/fusion/model_functions.h>

#include "private/distributed_file_downloader/storage.h"
#include "private/distributed_file_downloader/worker.h"
#include "private/distributed_file_downloader/resource_pool_peer_manager.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

//-------------------------------------------------------------------------------------------------

FileInformation::FileInformation()
{
}

FileInformation::FileInformation(const QString& fileName):
    name(fileName)
{
}

bool FileInformation::isValid() const
{
    return !name.isEmpty();
}

//-------------------------------------------------------------------------------------------------

class DownloaderPrivate: public QObject
{
    Downloader* const q_ptr;
    Q_DECLARE_PUBLIC(Downloader)

public:
    DownloaderPrivate(Downloader* q);

    void createWorker(const QString& fileName);

private:
    QnMutex mutex;
    QScopedPointer<Storage> storage;
    QHash<QString, Worker*> workers;
    AbstractPeerManagerFactory* peerManagerFactory = nullptr;
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
        Q_Q(Downloader);
        auto worker = new Worker(
            fileName, storage.data(), peerManagerFactory->createPeerManager());
        workers[fileName] = worker;
        worker->start();
    }
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

    if (peerManagerFactory)
        d->peerManagerFactory = peerManagerFactory;
    else
        d->peerManagerFactory = new ResourcePoolPeerManagerFactory(commonModule);

    for (const auto& fileName: d->storage->files())
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

Downloader::ErrorCode Downloader::addFile(const FileInformation& fileInformation)
{
    Q_D(Downloader);

    auto errorCode = d->storage->addFile(fileInformation);
    if (errorCode != ErrorCode::noError)
        return errorCode;

    executeInThread(thread(),
        [this, fileName = fileInformation.name]
        {
            Q_D(Downloader);
            d->createWorker(fileName);
        });

    return errorCode;
}

Downloader::ErrorCode Downloader::updateFileInformation(
    const QString& fileName,
    int size,
    const QByteArray& md5)
{
    Q_D(Downloader);
    return d->storage->updateFileInformation(fileName, size, md5);
}

Downloader::ErrorCode Downloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    Q_D(Downloader);
    return d->storage->readFileChunk(fileName, chunkIndex, buffer);
}

Downloader::ErrorCode Downloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& buffer)
{
    Q_D(Downloader);
    return d->storage->writeFileChunk(fileName, chunkIndex, buffer);
}

Downloader::ErrorCode Downloader::deleteFile(
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

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, Status)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Downloader, ErrorCode)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((FileInformation), (json), _Fields)

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
