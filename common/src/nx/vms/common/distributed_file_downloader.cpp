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

using namespace distributed_file_downloader;

//-------------------------------------------------------------------------------------------------

DownloaderFileInformation::DownloaderFileInformation()
{
}

DownloaderFileInformation::DownloaderFileInformation(const QString& fileName):
    name(fileName)
{
}

bool DownloaderFileInformation::isValid() const
{
    return !name.isEmpty();
}

//-------------------------------------------------------------------------------------------------

class DistributedFileDownloaderPrivate: public QObject
{
    DistributedFileDownloader* const q_ptr;
    Q_DECLARE_PUBLIC(DistributedFileDownloader)

public:
    DistributedFileDownloaderPrivate(DistributedFileDownloader* q);

    void createWorker(const QString& fileName);

private:
    QnMutex mutex;
    QScopedPointer<Storage> storage;
    QHash<QString, Worker*> workers;
    AbstractPeerManagerFactory* peerManagerFactory = nullptr;
};

DistributedFileDownloaderPrivate::DistributedFileDownloaderPrivate(DistributedFileDownloader* q):
    QObject(q),
    q_ptr(q)
{
}

void DistributedFileDownloaderPrivate::createWorker(const QString& fileName)
{
    QnMutexLocker lock(&mutex);

    NX_ASSERT(!workers.contains(fileName));
    if (workers.contains(fileName))
        return;

    const auto status = storage->fileInformation(fileName).status;

    if (status != DownloaderFileInformation::Status::downloaded
        && status != DownloaderFileInformation::Status::uploading)
    {
        Q_Q(DistributedFileDownloader);
        auto worker = new Worker(
            fileName, storage.data(), peerManagerFactory->createPeerManager());
        workers[fileName] = worker;
        worker->start();
    }
}

//-------------------------------------------------------------------------------------------------

DistributedFileDownloader::DistributedFileDownloader(
    const QDir& downloadsDirectory,
    QnCommonModule* commonModule,
    AbstractPeerManagerFactory* peerManagerFactory,
    QObject* parent)
    :
    QObject(parent),
    QnCommonModuleAware(commonModule),
    d_ptr(new DistributedFileDownloaderPrivate(this))
{
    Q_D(DistributedFileDownloader);
    d->storage.reset(new Storage(downloadsDirectory));

    if (peerManagerFactory)
        d->peerManagerFactory = peerManagerFactory;
    else
        d->peerManagerFactory = new ResourcePoolPeerManagerFactory(commonModule);

    for (const auto& fileName: d->storage->files())
        d->createWorker(fileName);
}

DistributedFileDownloader::~DistributedFileDownloader()
{
    Q_D(DistributedFileDownloader);
    qDeleteAll(d->workers);
}

QStringList DistributedFileDownloader::files() const
{
    Q_D(const DistributedFileDownloader);
    return d->storage->files();
}

QString DistributedFileDownloader::filePath(const QString& fileName) const
{
    Q_D(const DistributedFileDownloader);
    return d->storage->filePath(fileName);
}

DownloaderFileInformation DistributedFileDownloader::fileInformation(
    const QString& fileName) const
{
    Q_D(const DistributedFileDownloader);
    return d->storage->fileInformation(fileName);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::addFile(
    const DownloaderFileInformation& fileInformation)
{
    Q_D(DistributedFileDownloader);

    auto errorCode = d->storage->addFile(fileInformation);
    if (errorCode != ErrorCode::noError)
        return errorCode;

    executeInThread(thread(),
        [this, fileName = fileInformation.name]
        {
            Q_D(DistributedFileDownloader);
            d->createWorker(fileName);
        });

    return errorCode;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::updateFileInformation(
    const QString& fileName,
    int size,
    const QByteArray& md5)
{
    Q_D(DistributedFileDownloader);
    return d->storage->updateFileInformation(fileName, size, md5);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    Q_D(DistributedFileDownloader);
    return d->storage->readFileChunk(fileName, chunkIndex, buffer);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& buffer)
{
    Q_D(DistributedFileDownloader);
    return d->storage->writeFileChunk(fileName, chunkIndex, buffer);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::deleteFile(
    const QString& fileName,
    bool deleteData)
{
    Q_D(DistributedFileDownloader);
    return d->storage->deleteFile(fileName, deleteData);
}

QVector<QByteArray> DistributedFileDownloader::getChunkChecksums(const QString& fileName)
{
    Q_D(DistributedFileDownloader);
    return d->storage->getChunkChecksums(fileName);
}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(DownloaderFileInformation, Status)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(DistributedFileDownloader, ErrorCode)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((DownloaderFileInformation), (json), _Fields)

} // namespace common
} // namespace vms
} // namespace nx
