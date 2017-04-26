#include "distributed_file_downloader.h"

#include <QtCore/QHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>

namespace {

static constexpr qint64 kDefaultChunkSize = 1024 * 1024;
static const QString kMetadataSuffix = lit(".vmsdownload");

} // namespace

namespace nx {
namespace vms {
namespace common {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(DownloaderFileInformation, Status)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((DownloaderFileInformation), (json), _Fields)

namespace detail {

struct FileMetadata: DownloaderFileInformation
{
    FileMetadata()
    {
    }

    FileMetadata(const DownloaderFileInformation& fileInformation):
        DownloaderFileInformation(fileInformation)
    {
    }

    QVector<QByteArray> chunkChecksums;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FileMetadata,
    (json),
    DownloaderFileInformation_Fields (chunkChecksums))

} // namespace detail

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

    bool saveMetadata(const detail::FileMetadata& fileInformation);
    detail::FileMetadata loadMetadata(const QString& fileName);
    detail::FileMetadata fileMetadata(const QString& fileName) const;
    bool reserveSpace(const QString& fileName, const qint64 size);
    void checkDownloadCompleted(detail::FileMetadata& fileInformation);
    DistributedFileDownloader::ErrorCode loadDownload(const QString& fileName);
    void findDownloads();

    static QString metadataFileName(const QString& fileName);
    static qint64 chunkSize(qint64 fileSize, int chunkIndex, qint64 chunkSize);
    static QVector<QByteArray> calculateChecksums(const QString& fileName, qint64 chunkSize);

private:
    QDir downloadsDir;
    QHash<QString, detail::FileMetadata> fileInformationByName;
};

DistributedFileDownloaderPrivate::DistributedFileDownloaderPrivate(DistributedFileDownloader* q):
    QObject(q),
    q_ptr(q)
{
}

bool DistributedFileDownloaderPrivate::saveMetadata(
    const detail::FileMetadata& fileInformation)
{
    Q_Q(DistributedFileDownloader);

    const auto fileName = metadataFileName(q->filePath(fileInformation.name));

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;

    QByteArray buffer = QJson::serialized(fileInformation);
    if (file.write(buffer) != buffer.size())
    {
        file.close();
        file.remove();
        return false;
    }

    return true;
}

detail::FileMetadata DistributedFileDownloaderPrivate::loadMetadata(
    const QString& fileName)
{
    auto metadataFileName = fileName;
    if (!metadataFileName.endsWith(kMetadataSuffix))
        metadataFileName = this->metadataFileName(fileName);

    detail::FileMetadata fileInformation;

    QFile file(metadataFileName);
    if (!file.open(QFile::ReadOnly))
        return fileInformation;

    constexpr int kMaxMetadataFileSize = 1024 * 16;

    if (file.size() > kMaxMetadataFileSize)
        return fileInformation;

    const auto data = file.readAll();
    QJson::deserialize(data, &fileInformation);

    const auto previousStatus = fileInformation.status;
    checkDownloadCompleted(fileInformation);

    if (previousStatus != fileInformation.status)
        saveMetadata(fileInformation);

    return fileInformation;
}

detail::FileMetadata DistributedFileDownloaderPrivate::fileMetadata(const QString& fileName) const
{
    return fileInformationByName.value(fileName);
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
    detail::FileMetadata& fileInformation)
{
    Q_Q(DistributedFileDownloader);

    if (fileInformation.size < 0 || fileInformation.chunkSize <= 0)
    {
        fileInformation.status = DownloaderFileInformation::Status::downloading;
        return;
    }

    const int chunkCount = DistributedFileDownloader::calculateChunkCount(
        fileInformation.size, fileInformation.chunkSize);

    if (chunkCount != fileInformation.downloadedChunks.size())
    {
        fileInformation.status = DownloaderFileInformation::Status::corrupted;
        return;
    }

    for (int i = 0; i < fileInformation.downloadedChunks.size(); ++i)
    {
        if (!fileInformation.downloadedChunks.testBit(i))
            return;
    }

    const auto path = q->filePath(fileInformation.name);

    if (DistributedFileDownloader::calculateMd5(path) != fileInformation.md5)
    {
        fileInformation.status = DownloaderFileInformation::Status::corrupted;
        return;
    }

    fileInformation.status = DownloaderFileInformation::Status::downloaded;
    fileInformation.chunkChecksums = calculateChecksums(path, fileInformation.size);
}


DistributedFileDownloader::ErrorCode DistributedFileDownloaderPrivate::loadDownload(
    const QString& fileName)
{
    auto fileInfo = loadMetadata(fileName);

    if (!fileInfo.isValid())
        return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

    Q_Q(DistributedFileDownloader);
    return q->addFile(fileInfo);
}

void DistributedFileDownloaderPrivate::findDownloads()
{
    if (!downloadsDir.exists())
        return;

    for (const auto& entry: downloadsDir.entryInfoList({lit("*") + kMetadataSuffix}, QDir::Files))
    {
        auto fileName = entry.absoluteFilePath();
        fileName.truncate(fileName.size() - kMetadataSuffix.size());

        if (QFileInfo(fileName).isFile())
            loadDownload(fileName);
    }
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

QVector<QByteArray> DistributedFileDownloaderPrivate::calculateChecksums(
    const QString& fileName, qint64 chunkSize)
{
    QVector<QByteArray> result;

    QFile file(fileName);

    if (!file.open(QFile::ReadOnly))
        return result;

    const qint64 fileSize = file.size();
    const int chunkCount = DistributedFileDownloader::calculateChunkCount(fileSize, chunkSize);
    result.resize(chunkCount);

    for (int i = 0; i < chunkCount; ++i)
    {
        const auto data = file.read(chunkSize);
        if (data.size() != DistributedFileDownloaderPrivate::chunkSize(fileSize, i, chunkSize))
            return QVector<QByteArray>();

        result[i] = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    }

    return result;
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
    d->downloadsDir = downloadsDirectory;
    d->findDownloads();
}

DistributedFileDownloader::~DistributedFileDownloader()
{
}

QStringList DistributedFileDownloader::files() const
{
    Q_D(const DistributedFileDownloader);
    return d->fileInformationByName.keys();
}

QString DistributedFileDownloader::filePath(const QString& fileName) const
{
    Q_D(const DistributedFileDownloader);
    return d->downloadsDir.absoluteFilePath(fileName);
}

DownloaderFileInformation DistributedFileDownloader::fileInformation(
    const QString& fileName) const
{
    Q_D(const DistributedFileDownloader);
    return d->fileMetadata(fileName);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloader::addFile(
    const DownloaderFileInformation& fileInformation)
{
    Q_D(DistributedFileDownloader);

    if (d->fileInformationByName.contains(fileInformation.name))
        return ErrorCode::fileAlreadyExists;

    detail::FileMetadata info = fileInformation;
    const auto path = filePath(fileInformation.name);

    if (info.chunkSize <= 0)
        info.chunkSize = kDefaultChunkSize;

    if (fileInformation.status == DownloaderFileInformation::Status::downloaded)
    {
        QFile file(path);

        if (!file.exists())
            return ErrorCode::fileDoesNotExist;

        const auto md5 = calculateMd5(path);
        if (md5.isEmpty())
            return ErrorCode::ioError;

        if (info.md5.isEmpty())
            info.md5 = md5;
        else if (info.md5 != md5)
            return ErrorCode::invalidChecksum;

        const auto size = calculateFileSize(path);
        if (size < 0)
            return ErrorCode::ioError;

        if (info.size < 0)
            info.size = size;
        else if (info.size != size)
            return ErrorCode::invalidFileSize;

        const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
        info.chunkChecksums = d->calculateChecksums(path, info.chunkSize);
        if (info.chunkChecksums.size() != chunkCount)
            return ErrorCode::ioError;

        info.downloadedChunks.fill(true, chunkCount);
    }
    else
    {
        if (!info.md5.isEmpty() && calculateMd5(path) == info.md5)
        {
            info.status = DownloaderFileInformation::Status::downloaded;
            info.size = calculateFileSize(path);
            if (info.size < 0)
                return ErrorCode::ioError;

            const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
            info.chunkChecksums = d->calculateChecksums(path, info.chunkSize);
            if (info.chunkChecksums.size() != chunkCount)
                return ErrorCode::ioError;

            info.downloadedChunks.fill(true, calculateChunkCount(info.size, info.chunkSize));
        }
        else
        {
            info.status = DownloaderFileInformation::Status::downloading;
            if (info.size >= 0)
            {
                const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
                info.downloadedChunks.resize(chunkCount);
                info.chunkChecksums.resize(chunkCount);
            }

            if (!d->reserveSpace(path, info.size >= 0 ? info.size : 0))
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

    QFile file(filePath(it->name));
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

    if (it->status == DownloaderFileInformation::Status::downloaded)
        return ErrorCode::ioError;

    QFile file(filePath(it->name));
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

    const auto path = filePath(it->name);

    if (deleteData && QFile::exists(path))
    {
        if (!QFile::remove(path))
            return ErrorCode::ioError;
    }

    const auto metadataFileName = d->metadataFileName(path);
    if (QFile::exists(metadataFileName))
    {
        if (!QFile::remove(metadataFileName))
            return ErrorCode::ioError;
    }

    d->fileInformationByName.erase(it);

    return ErrorCode::noError;
}

QVector<QByteArray> DistributedFileDownloader::getChunkChecksums(const QString& fileName)
{
    Q_D(DistributedFileDownloader);

    const auto& fileInfo = d->fileMetadata(fileName);
    if (!fileInfo.isValid())
        return QVector<QByteArray>();

    return fileInfo.chunkChecksums;
}

QByteArray DistributedFileDownloader::calculateMd5(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result();
}

qint64 DistributedFileDownloader::calculateFileSize(const QString& filePath)
{
    QFile file(filePath);
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
