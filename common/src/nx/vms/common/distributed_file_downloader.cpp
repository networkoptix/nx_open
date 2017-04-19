#include "distributed_file_downloader.h"

#include <QtCore/QHash>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

namespace {

static constexpr qint64 kDefaultChunkSize = 1024 * 1024;
static const QString kMetadataSuffix = lit(".vmsdownload");

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

    bool saveMetadata(const DistributedFileDownloader::FileInformation& fileInformation);
    DistributedFileDownloader::FileInformation loadMetadata(const QString& fileName);
    bool reserveSpace(const QString& fileName, const qint64 size);
    void checkDownloadCompleted(DistributedFileDownloader::FileInformation& fileInformation);

    static QString metadataFileName(const QString& fileName);
    static qint64 chunkSize(qint64 fileSize, int chunkIndex, qint64 chunkSize);

private:
    QHash<QString, DistributedFileDownloader::FileInformation> fileInformationByName;
};

DistributedFileDownloaderPrivate::DistributedFileDownloaderPrivate(DistributedFileDownloader* q):
    QObject(q),
    q_ptr(q)
{
}

bool DistributedFileDownloaderPrivate::saveMetadata(
    const DistributedFileDownloader::FileInformation& fileInformation)
{
    const auto fileName = metadataFileName(fileInformation.name);

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;

    // TODO: serialize and write

    return true;
}

DistributedFileDownloader::FileInformation DistributedFileDownloaderPrivate::loadMetadata(
    const QString& fileName)
{
    auto metadataFileName = fileName;
    if (!metadataFileName.endsWith(kMetadataSuffix))
        metadataFileName = this->metadataFileName(fileName);

    DistributedFileDownloader::FileInformation fileInformation;

    QFile file(metadataFileName);
    if (!file.open(QFile::ReadOnly))
        return fileInformation;

    // TODO: read and deserialize

    return fileInformation;
}

bool DistributedFileDownloaderPrivate::reserveSpace(const QString& fileName, const qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::WriteOnly))
        return false;

    if (!file.resize(size))
        return false;

    return true;
}

void DistributedFileDownloaderPrivate::checkDownloadCompleted(
    DistributedFileDownloader::FileInformation& fileInformation)
{
    for (int i = 0; i < fileInformation.downloadedChunks.size(); ++i)
    {
        if (!fileInformation.downloadedChunks.testBit(i))
            return;
    }

    if (DistributedFileDownloader::calculateMd5(fileInformation.name) == fileInformation.md5)
        fileInformation.status = DistributedFileDownloader::FileStatus::downloaded;
    else
        fileInformation.status = DistributedFileDownloader::FileStatus::corrupted;
}

QString DistributedFileDownloaderPrivate::metadataFileName(const QString& fileName)
{
    if (fileName.isEmpty())
        return QString();

    return fileName + kMetadataSuffix;
}

qint64 DistributedFileDownloaderPrivate::chunkSize(
    qint64 fileSize, int chunkIndex, qint64 chunkSize)
{
    if (fileSize < 0 || chunkIndex < 0)
        return -1;

    const int chunkCount = DistributedFileDownloader::calculateChunkCount(fileSize, chunkSize);
    if (chunkIndex >= chunkCount)
        return -1;

    return chunkIndex < chunkCount - 1
        ? chunkSize
        : fileSize - chunkSize * (chunkCount - 1);
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
    Q_D(DistributedFileDownloader);

    if (d->fileInformationByName.contains(fileInformation.name))
        return ErrorCode::fileAlreadyExists;

    FileInformation info = fileInformation;

    if (info.chunkSize <= 0)
        info.chunkSize = kDefaultChunkSize;

    if (fileInformation.status == FileStatus::downloaded)
    {
        if (!QFile::exists(fileInformation.name))
            return ErrorCode::fileDoesNotExist;

        const auto md5 = calculateMd5(info.name);
        if (md5.isEmpty())
            return ErrorCode::ioError;

        if (info.md5.isEmpty())
            info.md5 = md5;
        else if (info.md5 != md5)
            return ErrorCode::invalidChecksum;

        const auto size = calculateFileSize(info.name);
        if (size < 0)
            return ErrorCode::ioError;

        if (info.size < 0)
            info.size = size;
        else if (info.size != size)
            return ErrorCode::invalidFileSize;

        info.downloadedChunks.fill(true, calculateChunkCount(info.size, info.chunkSize));
    }
    else
    {
        if (!info.md5.isEmpty() && calculateMd5(info.name) == info.md5)
        {
            info.status = FileStatus::downloaded;
            info.size = calculateFileSize(info.name);
            if (info.size < 0)
                return ErrorCode::ioError;

            info.downloadedChunks.fill(true, calculateChunkCount(info.size, info.chunkSize));
        }
        else
        {
            info.status = FileStatus::downloading;
            if (info.size >= 0)
                info.downloadedChunks.resize(calculateChunkCount(info.size, info.chunkSize));

            if (!d->reserveSpace(info.name, info.size >= 0 ? info.size : 0))
                return ErrorCode::noFreeSpace;
        }
    }

    if (!d->saveMetadata(info))
        return ErrorCode::ioError;

    d->fileInformationByName[fileInformation.name] = info;

    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::readFileChunk(
    const QString& fileName,
    int chunkIndex,
    QByteArray& buffer)
{
    Q_D(DistributedFileDownloader);

    auto it = d->fileInformationByName.find(fileName);
    if (it == d->fileInformationByName.end())
        return ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size())
        return ErrorCode::invalidChunkIndex;

    if (!it->downloadedChunks.testBit(chunkIndex))
        return ErrorCode::invalidChunkIndex;

    QFile file(it->name);
    if (!file.open(QFile::ReadOnly))
        return ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return ErrorCode::ioError;

    const qint64 bytesToRead = std::min(it->chunkSize, it->size - file.pos());
    buffer = file.read(bytesToRead);
    if (buffer.size() != bytesToRead)
        return ErrorCode::ioError;

    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::writeFileChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& buffer)
{
    Q_D(DistributedFileDownloader);

    auto it = d->fileInformationByName.find(fileName);
    if (it == d->fileInformationByName.end())
        return ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size() || it->size < 0)
        return ErrorCode::invalidChunkIndex;

    if (it->status == FileStatus::downloaded)
        return ErrorCode::ioError;

    QFile file(it->name);
    if (!file.open(QFile::ReadWrite)) //< ReadWrite because WriteOnly implies Truncate.
        return ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return ErrorCode::ioError;

    const qint64 bytesToWrite = d->chunkSize(it->size, chunkIndex, it->chunkSize);
    if (bytesToWrite < 0)
        return ErrorCode::ioError;

    if (bytesToWrite != buffer.size())
        return ErrorCode::invalidChunkSize;

    if (file.write(buffer.data(), bytesToWrite) != bytesToWrite)
        return ErrorCode::ioError;

    file.close();

    it->downloadedChunks.setBit(chunkIndex);
    d->checkDownloadCompleted(it.value());
    d->saveMetadata(it.value());

    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::deleteFile(
    const QString& fileName,
    bool deleteData)
{
    Q_D(DistributedFileDownloader);

    auto it = d->fileInformationByName.find(fileName);
    if (it == d->fileInformationByName.end())
        return ErrorCode::fileDoesNotExist;

    if (deleteData && QFile::exists(it->name))
    {
        if (!QFile::remove(it->name))
            return ErrorCode::ioError;
    }

    const auto metadataFileName = d->metadataFileName(it->name);
    if (QFile::exists(metadataFileName))
    {
        if (!QFile::remove(metadataFileName))
            return ErrorCode::ioError;
    }

    d->fileInformationByName.erase(it);

    return ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::findDownloads(
    const QString& path)
{
    return ErrorCode::noError;
}

QByteArray DistributedFileDownloader::calculateMd5(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result();
}

qint64 DistributedFileDownloader::calculateFileSize(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return -1;

    return file.size();
}

int DistributedFileDownloader::calculateChunkCount(qint64 fileSize, qint64 chunkSize)
{
    if (chunkSize <= 0 || fileSize < 0)
        return -1;

    return (fileSize + chunkSize - 1) / chunkSize;
}

} // namespace common
} // namespace vms
} // namespace nx
