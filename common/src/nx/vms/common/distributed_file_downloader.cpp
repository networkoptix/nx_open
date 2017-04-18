#include "distributed_file_downloader.h"

#include <QtCore/QHash>

namespace {

static constexpr qint64 kDefaultChunkSize = 1024 * 1024;

} // namespace

namespace nx {
namespace vms {
namespace common {

DistributedFileDownloader::FileInformation::FileInformation()
{
}

DistributedFileDownloader::FileInformation::FileInformation(const QString& fileName):
    name(fileName)
{
}

//-------------------------------------------------------------------------------------------------

class DistributedFileDownloaderPrivate: public QObject
{
    DistributedFileDownloader* const q_ptr;
    Q_DECLARE_PUBLIC(DistributedFileDownloader)

public:
    DistributedFileDownloaderPrivate(DistributedFileDownloader* q);

private:
    QHash<QString, DistributedFileDownloader::FileInformation> fileInformationByName;
};

DistributedFileDownloaderPrivate::DistributedFileDownloaderPrivate(DistributedFileDownloader* q):
    QObject(q),
    q_ptr(q)
{
}

//-------------------------------------------------------------------------------------------------

DistributedFileDownloader::DistributedFileDownloader(QObject* parent):
    QObject(parent),
    d_ptr(new DistributedFileDownloaderPrivate(this))
{
}

DistributedFileDownloader::~DistributedFileDownloader()
{
}

QStringList DistributedFileDownloader::files() const
{
    Q_D(const DistributedFileDownloader);
    return d->fileInformationByName.keys();
}

DistributedFileDownloader::FileInformation DistributedFileDownloader::fileInformation(
    const QString& fileName) const
{
    Q_D(const DistributedFileDownloader);
    return d->fileInformationByName.value(fileName);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::addFile(
    const FileInformation& fileInformation)
{
    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& data)
{
    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::deleteFile(
    const QString& fileName,
    bool deleteData)
{
    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::findDownloads(
    const QString& path)
{
    return ErrorCode::noError;
}

} // namespace common
} // namespace vms
} // namespace nx
