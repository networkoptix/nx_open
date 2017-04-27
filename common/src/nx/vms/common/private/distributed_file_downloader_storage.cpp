#include "distributed_file_downloader_storage.h"

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

QN_FUSION_DECLARE_FUNCTIONS(FileMetadata, (json))

DistributedFileDownloaderStorage::DistributedFileDownloaderStorage(
    const QDir& downloadsDirectory)
    :
    m_downloadsDirectory(downloadsDirectory)
{
    findDownloads();
}

QStringList DistributedFileDownloaderStorage::files() const
{
    QnMutexLocker lock(&m_mutex);
    return m_fileInformationByName.keys();
}

QString DistributedFileDownloaderStorage::filePath(const QString& fileName) const
{
    return m_downloadsDirectory.absoluteFilePath(fileName);
}

DownloaderFileInformation DistributedFileDownloaderStorage::fileInformation(
    const QString& fileName) const
{
    QnMutexLocker lock(&m_mutex);
    return m_fileInformationByName.value(fileName);
}

DistributedFileDownloader::ErrorCode DistributedFileDownloaderStorage::addFile(
    const DownloaderFileInformation& fileInfo)
{
    QnMutexLocker lock(&m_mutex);

    if (m_fileInformationByName.contains(fileInfo.name))
        return DistributedFileDownloader::ErrorCode::fileAlreadyExists;

    FileMetadata info = fileInfo;
    const auto path = filePath(fileInfo.name);

    if (info.chunkSize <= 0)
        info.chunkSize = kDefaultChunkSize;

    if (fileInfo.status == DownloaderFileInformation::Status::downloaded)
    {
        QFile file(path);

        if (!file.exists())
            return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

        const auto md5 = calculateMd5(path);
        if (md5.isEmpty())
            return DistributedFileDownloader::ErrorCode::ioError;

        if (info.md5.isEmpty())
            info.md5 = md5;
        else if (info.md5 != md5)
            return DistributedFileDownloader::ErrorCode::invalidChecksum;

        const auto size = calculateFileSize(path);
        if (size < 0)
            return DistributedFileDownloader::ErrorCode::ioError;

        if (info.size < 0)
            info.size = size;
        else if (info.size != size)
            return DistributedFileDownloader::ErrorCode::invalidFileSize;

        const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
        info.chunkChecksums = calculateChecksums(path, info.chunkSize);
        if (info.chunkChecksums.size() != chunkCount)
            return DistributedFileDownloader::ErrorCode::ioError;

        info.downloadedChunks.fill(true, chunkCount);
    }
    else
    {
        if (!info.md5.isEmpty() && calculateMd5(path) == info.md5)
        {
            info.status = DownloaderFileInformation::Status::downloaded;
            info.size = calculateFileSize(path);
            if (info.size < 0)
                return DistributedFileDownloader::ErrorCode::ioError;

            const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
            info.chunkChecksums = calculateChecksums(path, info.chunkSize);
            if (info.chunkChecksums.size() != chunkCount)
                return DistributedFileDownloader::ErrorCode::ioError;

            info.downloadedChunks.fill(true, calculateChunkCount(info.size, info.chunkSize));
        }
        else
        {
            if (info.status == DownloaderFileInformation::Status::notFound)
                info.status = DownloaderFileInformation::Status::downloading;

            if (info.status == DownloaderFileInformation::Status::uploading)
            {
                if (info.size < 0)
                    return DistributedFileDownloader::ErrorCode::invalidFileSize;

                if (info.md5.isEmpty())
                    return DistributedFileDownloader::ErrorCode::invalidChecksum;
            }

            if (info.size >= 0)
            {
                const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
                info.downloadedChunks.resize(chunkCount);
                info.chunkChecksums.resize(chunkCount);
            }

            if (!reserveSpace(path, info.size >= 0 ? info.size : 0))
                return DistributedFileDownloader::ErrorCode::noFreeSpace;

            checkDownloadCompleted(info);
        }
    }

    if (!saveMetadata(info))
        return DistributedFileDownloader::ErrorCode::ioError;

    m_fileInformationByName.insert(fileInfo.name, info);

    return DistributedFileDownloader::ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloaderStorage::readFileChunk(
    const QString& fileName, int chunkIndex, QByteArray& buffer)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size())
        return DistributedFileDownloader::ErrorCode::invalidChunkIndex;

    if (!it->downloadedChunks.testBit(chunkIndex))
        return DistributedFileDownloader::ErrorCode::invalidChunkIndex;

    QFile file(filePath(it->name));
    if (!file.open(QFile::ReadOnly))
        return DistributedFileDownloader::ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return DistributedFileDownloader::ErrorCode::ioError;

    const qint64 bytesToRead = std::min(it->chunkSize, it->size - file.pos());
    buffer = file.read(bytesToRead);
    if (buffer.size() != bytesToRead)
        return DistributedFileDownloader::ErrorCode::ioError;

    return DistributedFileDownloader::ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloaderStorage::writeFileChunk(
    const QString& fileName, int chunkIndex, const QByteArray& buffer)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size() || it->size < 0)
        return DistributedFileDownloader::ErrorCode::invalidChunkIndex;

    if (it->status == DownloaderFileInformation::Status::downloaded)
        return DistributedFileDownloader::ErrorCode::ioError;

    QFile file(filePath(it->name));
    if (!file.open(QFile::ReadWrite)) //< ReadWrite because WriteOnly implies Truncate.
        return DistributedFileDownloader::ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return DistributedFileDownloader::ErrorCode::ioError;

    const qint64 bytesToWrite = calculateChunkSize(it->size, chunkIndex, it->chunkSize);
    if (bytesToWrite < 0)
        return DistributedFileDownloader::ErrorCode::ioError;

    if (bytesToWrite != buffer.size())
        return DistributedFileDownloader::ErrorCode::invalidChunkSize;

    if (file.write(buffer.data(), bytesToWrite) != bytesToWrite)
        return DistributedFileDownloader::ErrorCode::ioError;

    file.close();

    it->downloadedChunks.setBit(chunkIndex);
    checkDownloadCompleted(it.value());
    saveMetadata(it.value());

    return DistributedFileDownloader::ErrorCode::noError;
}

DistributedFileDownloader::ErrorCode DistributedFileDownloaderStorage::deleteFile(
    const QString& fileName, bool deleteData)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

    const auto path = filePath(it->name);

    if (deleteData && QFile::exists(path))
    {
        if (!QFile::remove(path))
            return DistributedFileDownloader::ErrorCode::ioError;
    }

    const auto metadataFileName = this->metadataFileName(path);
    if (QFile::exists(metadataFileName))
    {
        if (!QFile::remove(metadataFileName))
            return DistributedFileDownloader::ErrorCode::ioError;
    }

    m_fileInformationByName.erase(it);

    return DistributedFileDownloader::ErrorCode::noError;
}

QVector<QByteArray> DistributedFileDownloaderStorage::getChunkChecksums(
    const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);

    const auto& fileInfo = fileMetadata(fileName);
    if (!fileInfo.isValid())
        return QVector<QByteArray>();

    return fileInfo.chunkChecksums;
}

void DistributedFileDownloaderStorage::findDownloads()
{
    if (!m_downloadsDirectory.exists())
        return;

    for (const auto& entry: m_downloadsDirectory.entryInfoList(
        {lit("*") + kMetadataSuffix}, QDir::Files))
    {
        auto fileName = entry.absoluteFilePath();
        fileName.truncate(fileName.size() - kMetadataSuffix.size());

        if (QFileInfo(fileName).isFile())
            loadDownload(fileName);
    }
}

QByteArray DistributedFileDownloaderStorage::calculateMd5(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result();
}

qint64 DistributedFileDownloaderStorage::calculateFileSize(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return -1;

    return file.size();
}

int DistributedFileDownloaderStorage::calculateChunkCount(qint64 fileSize, qint64 chunkSize)
{
    if (chunkSize <= 0 || fileSize < 0)
        return -1;

    return (fileSize + chunkSize - 1) / chunkSize;
}

bool DistributedFileDownloaderStorage::saveMetadata(const FileMetadata& fileInformation)
{
    const auto fileName = metadataFileName(filePath(fileInformation.name));

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

FileMetadata DistributedFileDownloaderStorage::loadMetadata(const QString& fileName)
{
    auto metadataFileName = fileName;
    if (!metadataFileName.endsWith(kMetadataSuffix))
        metadataFileName = this->metadataFileName(fileName);

    FileMetadata fileInfo;

    QFile file(metadataFileName);
    if (!file.open(QFile::ReadOnly))
        return fileInfo;

    constexpr int kMaxMetadataFileSize = 1024 * 16;

    if (file.size() > kMaxMetadataFileSize)
        return fileInfo;

    const auto data = file.readAll();
    QJson::deserialize(data, &fileInfo);

    const auto previousStatus = fileInfo.status;
    checkDownloadCompleted(fileInfo);

    if (previousStatus != fileInfo.status)
        saveMetadata(fileInfo);

    return fileInfo;
}

FileMetadata DistributedFileDownloaderStorage::fileMetadata(const QString& fileName) const
{
    return m_fileInformationByName.value(fileName);
}


DistributedFileDownloader::ErrorCode DistributedFileDownloaderStorage::loadDownload(
    const QString& fileName)
{
    auto fileInfo = loadMetadata(fileName);

    if (!fileInfo.isValid())
        return DistributedFileDownloader::ErrorCode::fileDoesNotExist;

    return addFile(fileInfo);
}

void DistributedFileDownloaderStorage::checkDownloadCompleted(FileMetadata& fileInfo)
{
    if (fileInfo.size < 0 || fileInfo.chunkSize <= 0)
    {
        fileInfo.status = DownloaderFileInformation::Status::downloading;
        return;
    }

    const int chunkCount = calculateChunkCount(fileInfo.size, fileInfo.chunkSize);

    if (chunkCount != fileInfo.downloadedChunks.size())
    {
        fileInfo.status = DownloaderFileInformation::Status::corrupted;
        return;
    }

    for (int i = 0; i < fileInfo.downloadedChunks.size(); ++i)
    {
        if (!fileInfo.downloadedChunks.testBit(i))
            return;
    }

    const auto path = filePath(fileInfo.name);

    if (calculateMd5(path) != fileInfo.md5)
    {
        fileInfo.status = DownloaderFileInformation::Status::corrupted;
        return;
    }

    fileInfo.status = DownloaderFileInformation::Status::downloaded;
    fileInfo.chunkChecksums = calculateChecksums(path, fileInfo.size);
}

bool DistributedFileDownloaderStorage::reserveSpace(const QString& fileName, const qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadWrite))
        return false;

    if (!file.resize(size))
        return false;

    return true;
}

QString DistributedFileDownloaderStorage::metadataFileName(const QString& fileName)
{
    if (fileName.isEmpty())
        return QString();

    return fileName + kMetadataSuffix;
}

qint64 DistributedFileDownloaderStorage::calculateChunkSize(
    qint64 fileSize, int chunkIndex, qint64 chunkSize)
{
    if (fileSize < 0 || chunkIndex < 0)
        return -1;

    const int chunkCount = calculateChunkCount(fileSize, chunkSize);
    if (chunkIndex >= chunkCount)
        return -1;

    return chunkIndex < chunkCount - 1
        ? chunkSize
        : fileSize - chunkSize * (chunkCount - 1);
}

QVector<QByteArray> DistributedFileDownloaderStorage::calculateChecksums(
    const QString& fileName, qint64 chunkSize)
{
    QVector<QByteArray> result;

    QFile file(fileName);

    if (!file.open(QFile::ReadOnly))
        return result;

    const qint64 fileSize = file.size();
    const int chunkCount = calculateChunkCount(fileSize, chunkSize);
    result.resize(chunkCount);

    for (int i = 0; i < chunkCount; ++i)
    {
        const auto data = file.read(chunkSize);
        if (data.size() != calculateChunkSize(fileSize, i, chunkSize))
            return QVector<QByteArray>();

        result[i] = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    }

    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileMetadata, (json), DownloaderFileInformation_Fields (chunkChecksums))

} // namespace common
} // namespace vms
} // namespace nx
