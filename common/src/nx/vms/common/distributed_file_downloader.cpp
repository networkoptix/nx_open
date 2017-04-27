#include "distributed_file_downloader.h"

#include <nx/fusion/model_functions.h>

#include "private/distributed_file_downloader_storage.h"

namespace nx {
namespace vms {
namespace common {

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

private:
    QScopedPointer<DistributedFileDownloaderStorage> storage;
};

DistributedFileDownloaderPrivate::DistributedFileDownloaderPrivate(DistributedFileDownloader* q):
    QObject(q),
    q_ptr(q)
{
}

//-------------------------------------------------------------------------------------------------

DistributedFileDownloader::DistributedFileDownloader(
    const QDir& downloadsDirectory,
    QObject* parent)
    :
    QObject(parent),
    d_ptr(new DistributedFileDownloaderPrivate(this))
{
    Q_D(DistributedFileDownloader);
    d->storage.reset(new DistributedFileDownloaderStorage(downloadsDirectory));
}

DistributedFileDownloader::~DistributedFileDownloader()
{
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
    return d->storage->addFile(fileInformation);
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
