#include "storage.h"

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
namespace distributed_file_downloader {

QN_FUSION_DECLARE_FUNCTIONS(FileMetadata, (json))

Storage::Storage(
    const QDir& downloadsDirectory)
    :
    m_downloadsDirectory(downloadsDirectory)
{
    findDownloads();
}

QDir Storage::downloadsDirectory() const
{
    return m_downloadsDirectory;
}

QStringList Storage::files() const
{
    QnMutexLocker lock(&m_mutex);
    return m_fileInformationByName.keys();
}

QString Storage::filePath(const QString& fileName) const
{
    return m_downloadsDirectory.absoluteFilePath(fileName);
}

FileInformation Storage::fileInformation(
    const QString& fileName) const
{
    QnMutexLocker lock(&m_mutex);
    return m_fileInformationByName.value(fileName);
}

Downloader::ErrorCode Storage::addFile(
    const FileInformation& fileInfo)
{
    QnMutexLocker lock(&m_mutex);

    if (m_fileInformationByName.contains(fileInfo.name))
        return Downloader::ErrorCode::fileAlreadyExists;

    FileMetadata info = fileInfo;
    const auto path = filePath(fileInfo.name);

    if (info.chunkSize <= 0)
        info.chunkSize = kDefaultChunkSize;

    if (fileInfo.status == FileInformation::Status::downloaded)
    {
        QFile file(path);

        if (!file.exists())
            return Downloader::ErrorCode::fileDoesNotExist;

        const auto md5 = calculateMd5(path);
        if (md5.isEmpty())
            return Downloader::ErrorCode::ioError;

        if (info.md5.isEmpty())
            info.md5 = md5;
        else if (info.md5 != md5)
            return Downloader::ErrorCode::invalidChecksum;

        const auto size = calculateFileSize(path);
        if (size < 0)
            return Downloader::ErrorCode::ioError;

        if (info.size < 0)
            info.size = size;
        else if (info.size != size)
            return Downloader::ErrorCode::invalidFileSize;

        const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
        info.chunkChecksums = calculateChecksums(path, info.chunkSize);
        if (info.chunkChecksums.size() != chunkCount)
            return Downloader::ErrorCode::ioError;

        info.downloadedChunks.fill(true, chunkCount);
    }
    else
    {
        const auto fileDir = QFileInfo(path).absolutePath();
        if (!QDir(fileDir).exists())
        {
            if (!QDir().mkpath(fileDir))
                return Downloader::ErrorCode::ioError;
        }

        if (!info.md5.isEmpty() && calculateMd5(path) == info.md5)
        {
            info.status = FileInformation::Status::downloaded;
            info.size = calculateFileSize(path);
            if (info.size < 0)
                return Downloader::ErrorCode::ioError;

            const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
            info.chunkChecksums = calculateChecksums(path, info.chunkSize);
            if (info.chunkChecksums.size() != chunkCount)
                return Downloader::ErrorCode::ioError;

            info.downloadedChunks.fill(true, calculateChunkCount(info.size, info.chunkSize));
        }
        else
        {
            if (info.status == FileInformation::Status::notFound)
                info.status = FileInformation::Status::downloading;

            if (info.status == FileInformation::Status::uploading)
            {
                if (info.size < 0)
                    return Downloader::ErrorCode::invalidFileSize;

                if (info.md5.isEmpty())
                    return Downloader::ErrorCode::invalidChecksum;
            }

            if (info.size >= 0)
            {
                const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
                info.downloadedChunks.resize(chunkCount);
                info.chunkChecksums.resize(chunkCount);
            }

            if (!reserveSpace(path, info.size >= 0 ? info.size : 0))
                return Downloader::ErrorCode::noFreeSpace;

            checkDownloadCompleted(info);
        }
    }

    if (!saveMetadata(info))
        return Downloader::ErrorCode::ioError;

    m_fileInformationByName.insert(fileInfo.name, info);

    return Downloader::ErrorCode::noError;
}

Downloader::ErrorCode Storage::updateFileInformation(
    const QString& fileName, qint64 size, const QByteArray& md5)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return Downloader::ErrorCode::fileDoesNotExist;

    if (it->status == FileInformation::Status::downloaded)
        return Downloader::ErrorCode::fileAlreadyDownloaded;

    bool updated = false;
    bool resizeFailed = false;

    if (size >= 0 && it->size != size)
    {
        it->size = size;

        const int chunkCount = calculateChunkCount(size, it->chunkSize);
        it->downloadedChunks.resize(chunkCount);
        it->chunkChecksums.resize(chunkCount);
        resizeFailed = !QFile::resize(filePath(fileName), size);

        updated = true;
    }

    if (!md5.isEmpty() && it->md5 != md5)
    {
        it->md5 = md5;
        updated = true;
    }

    if (resizeFailed)
        return Downloader::ErrorCode::noFreeSpace;

    if (updated)
        checkDownloadCompleted(it.value());

    if (!saveMetadata(it.value()))
        return Downloader::ErrorCode::ioError;

    return Downloader::ErrorCode::noError;
}

Downloader::ErrorCode Storage::readFileChunk(
    const QString& fileName, int chunkIndex, QByteArray& buffer)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return Downloader::ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size())
        return Downloader::ErrorCode::invalidChunkIndex;

    if (!it->downloadedChunks.testBit(chunkIndex))
        return Downloader::ErrorCode::invalidChunkIndex;

    QFile file(filePath(it->name));
    if (!file.open(QFile::ReadOnly))
        return Downloader::ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return Downloader::ErrorCode::ioError;

    const qint64 bytesToRead = std::min(it->chunkSize, it->size - file.pos());
    buffer = file.read(bytesToRead);
    if (buffer.size() != bytesToRead)
        return Downloader::ErrorCode::ioError;

    return Downloader::ErrorCode::noError;
}

Downloader::ErrorCode Storage::writeFileChunk(
    const QString& fileName, int chunkIndex, const QByteArray& buffer)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return Downloader::ErrorCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size() || it->size < 0)
        return Downloader::ErrorCode::invalidChunkIndex;

    if (it->status == FileInformation::Status::downloaded)
        return Downloader::ErrorCode::ioError;

    QFile file(filePath(it->name));
    if (!file.open(QFile::ReadWrite)) //< ReadWrite because WriteOnly implies Truncate.
        return Downloader::ErrorCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return Downloader::ErrorCode::ioError;

    const qint64 bytesToWrite = calculateChunkSize(it->size, chunkIndex, it->chunkSize);
    if (bytesToWrite < 0)
        return Downloader::ErrorCode::ioError;

    if (bytesToWrite != buffer.size())
        return Downloader::ErrorCode::invalidChunkSize;

    if (file.write(buffer.data(), bytesToWrite) != bytesToWrite)
        return Downloader::ErrorCode::ioError;

    file.close();

    it->downloadedChunks.setBit(chunkIndex);
    checkDownloadCompleted(it.value());
    saveMetadata(it.value());

    return Downloader::ErrorCode::noError;
}

Downloader::ErrorCode Storage::deleteFile(
    const QString& fileName, bool deleteData)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return Downloader::ErrorCode::fileDoesNotExist;

    const auto path = filePath(it->name);

    if (deleteData && QFile::exists(path))
    {
        if (!QFile::remove(path))
            return Downloader::ErrorCode::ioError;
    }

    const auto metadataFileName = this->metadataFileName(path);
    if (QFile::exists(metadataFileName))
    {
        if (!QFile::remove(metadataFileName))
            return Downloader::ErrorCode::ioError;
    }


    QDir dir = QFileInfo(path).absoluteDir();
    while (dir != m_downloadsDirectory)
    {
        if (dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
        {
            const auto name = dir.dirName();

            if (!dir.cdUp())
                break;

            if (!dir.rmdir(name))
                break;
        }
    }

    m_fileInformationByName.erase(it);

    return Downloader::ErrorCode::noError;
}

QVector<QByteArray> Storage::getChunkChecksums(
    const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);

    const auto& fileInfo = fileMetadata(fileName);
    if (!fileInfo.isValid())
        return QVector<QByteArray>();

    return fileInfo.chunkChecksums;
}

Downloader::ErrorCode Storage::setChunkChecksums(
    const QString& fileName, const QVector<QByteArray>& chunkChecksums)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return Downloader::ErrorCode::fileDoesNotExist;

    if (it->chunkChecksums == chunkChecksums)
        return Downloader::ErrorCode::noError;

    if (it->status == FileInformation::Status::downloaded)
        return Downloader::ErrorCode::fileAlreadyDownloaded;

    if (it->size >= 0 && it->downloadedChunks.size() != chunkChecksums.size())
        return Downloader::ErrorCode::invalidChecksum;

    it->chunkChecksums = chunkChecksums;

    if (it->size < 0)
        return Downloader::ErrorCode::noError;

    auto actualChecksums = calculateChecksums(filePath(fileName), it->chunkSize);
    if (actualChecksums.size() != it->downloadedChunks.size())
        actualChecksums.resize(it->downloadedChunks.size());

    for (int i = 0; i < actualChecksums.size(); ++i)
    {
        if (actualChecksums[i] != chunkChecksums[i])
        {
            it->downloadedChunks[i] = false;
            it->status = FileInformation::Status::downloading;
        }
    }

    return Downloader::ErrorCode::noError;
}

void Storage::findDownloads()
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

QByteArray Storage::calculateMd5(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    return hash.result();
}

qint64 Storage::calculateFileSize(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return -1;

    return file.size();
}

int Storage::calculateChunkCount(qint64 fileSize, qint64 chunkSize)
{
    if (chunkSize <= 0 || fileSize < 0)
        return -1;

    return (fileSize + chunkSize - 1) / chunkSize;
}

QVector<QByteArray> Storage::calculateChecksums(const QString& filePath, qint64 chunkSize)
{
    QVector<QByteArray> result;

    QFile file(filePath);

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

bool Storage::saveMetadata(const FileMetadata& fileInformation)
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

FileMetadata Storage::loadMetadata(const QString& fileName)
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

FileMetadata Storage::fileMetadata(const QString& fileName) const
{
    return m_fileInformationByName.value(fileName);
}


Downloader::ErrorCode Storage::loadDownload(
    const QString& fileName)
{
    auto fileInfo = loadMetadata(fileName);

    if (!fileInfo.isValid())
        return Downloader::ErrorCode::fileDoesNotExist;

    return addFile(fileInfo);
}

void Storage::checkDownloadCompleted(FileMetadata& fileInfo)
{
    if (fileInfo.size < 0 || fileInfo.chunkSize <= 0)
    {
        fileInfo.status = FileInformation::Status::downloading;
        return;
    }

    const int chunkCount = calculateChunkCount(fileInfo.size, fileInfo.chunkSize);

    if (chunkCount != fileInfo.downloadedChunks.size())
    {
        fileInfo.status = FileInformation::Status::corrupted;
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
        fileInfo.status = FileInformation::Status::corrupted;
        return;
    }

    fileInfo.status = FileInformation::Status::downloaded;
    fileInfo.chunkChecksums = calculateChecksums(path, fileInfo.size);
}

bool Storage::reserveSpace(const QString& fileName, const qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadWrite))
        return false;

    if (!file.resize(size))
        return false;

    return true;
}

QString Storage::metadataFileName(const QString& fileName)
{
    if (fileName.isEmpty())
        return QString();

    return fileName + kMetadataSuffix;
}

qint64 Storage::calculateChunkSize(
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FileMetadata, (json), FileInformation_Fields (chunkChecksums))

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
