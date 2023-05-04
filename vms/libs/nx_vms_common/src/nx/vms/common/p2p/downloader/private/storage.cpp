// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage.h"

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/file_system.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/suppress_exceptions.h>

using namespace std::chrono;

namespace nx::vms::common::p2p::downloader {

namespace {

static constexpr qint64 kDefaultChunkSize = 1024 * 1024;
static const QString kMetadataSuffix = ".vmsdownload";
static const milliseconds kCleanupPeriod = 5min;

} // namespace

FileMetadata FileMetadata::fromFileInformation(
    const FileInformation& fileInformation,
    const QDir& defaultDownloadsDirectory)
{
    FileMetadata info = fileInformation;
    if (info.chunkSize <= 0)
        info.chunkSize = kDefaultChunkSize;

    if (info.absoluteDirectoryPath.isEmpty())
    {
        info.fullFilePath = defaultDownloadsDirectory.absoluteFilePath(info.name);
        info.absoluteDirectoryPath = QFileInfo(info.fullFilePath).absolutePath();
    }
    else if (info.fullFilePath.isEmpty())
    {
        info.fullFilePath =
            QDir(info.absoluteDirectoryPath).absoluteFilePath(QFileInfo(info.name).fileName());
    }

    return info;
}

QN_FUSION_DECLARE_FUNCTIONS(FileMetadata, (json))

Storage::Storage(const QDir& downloadsDirectory, QObject* parent):
    QObject(parent),
    m_downloadsDirectory(downloadsDirectory)
{
    QDir().mkpath(metadataDirectoryPath());
    /* Cleanup expired files every 5 mins. */
    const auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Storage::cleanupExpiredFiles);
    timer->start(kCleanupPeriod);

    connect(&m_loadDownloadsWatcher, &QFutureWatcher<void>::finished, this,
        &Storage::existingDownloadsLoaded);
}

Storage::~Storage()
{
    m_loadDownloadsWatcher.waitForFinished();
}

QDir Storage::downloadsDirectory() const
{
    return m_downloadsDirectory;
}

QString Storage::filePath(const QString& fileName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.cend())
        return QString();

    return it.value().fullFilePath;
}

QStringList Storage::files() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_fileInformationByName.keys();
}

FileInformation Storage::fileInformation(const QString& fileName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_fileInformationByName.value(fileName);
}

ResultCode Storage::addFile(FileInformation fileInformation, bool updateTouchTime)
{
    if (updateTouchTime)
        fileInformation.touchTime = QDateTime::currentMSecsSinceEpoch();

    return addFileInternal(fileInformation);
}

ResultCode Storage::moveFile(
    const QString& fileName, const QString& folderPath, const QString& type)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (it->absoluteDirectoryPath == folderPath)
        return ResultCode::ok;

    auto metadata = loadMetadata(fileName);
    const auto newFullFilePath = QDir(folderPath).absoluteFilePath(fileName);
    if (!QFile(it->fullFilePath).rename(newFullFilePath))
        return ResultCode::ioError;

    it->absoluteDirectoryPath = folderPath;
    it->fullFilePath = QDir(it->absoluteDirectoryPath).absoluteFilePath(fileName);
    it->userData = type;
    metadata.absoluteDirectoryPath = folderPath;
    metadata.fullFilePath = it->fullFilePath;
    metadata.userData = type;
    if (!NX_ASSERT(saveMetadata(metadata)))
        return ResultCode::ioError;

    return ResultCode::ok;
}

ResultCode Storage::addFileInternal(FileInformation fileInformation, bool loadingDownloads)
{
    if (fileInformation.status == FileInformation::Status::downloaded)
        return addDownloadedFile(fileInformation, loadingDownloads);
    return addNewFile(fileInformation, loadingDownloads);
}

ResultCode Storage::addDownloadedFile(
    const FileInformation& fileInformation, bool loadingDownloads)
{
    NX_ASSERT(fileInformation.status == FileInformation::Status::downloaded);

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!loadingDownloads && m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    if (m_fileInformationByName.contains(fileInformation.name))
        return ResultCode::fileAlreadyExists;

    FileMetadata info = FileMetadata::fromFileInformation(fileInformation, m_downloadsDirectory);
    if (!QFile(info.fullFilePath).exists())
    {
        NX_WARNING(
            this,
            "Add downloaded file (%1) failed. File not found.", info.fullFilePath);
        return ResultCode::fileDoesNotExist;
    }

    const auto md5 = calculateMd5(info.fullFilePath);
    if (md5.isEmpty())
        return ResultCode::ioError;

    if (info.md5.isEmpty())
        info.md5 = md5;
    else if (info.md5 != md5)
        return ResultCode::invalidChecksum;

    const auto size = calculateFileSize(info.fullFilePath);
    if (size < 0)
        return ResultCode::ioError;

    if (info.size < 0)
        info.size = size;
    else if (info.size != size)
        return ResultCode::invalidFileSize;

    const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
    info.downloadedChunks.fill(true, chunkCount);

    if (!saveMetadata(info))
    {
        NX_ERROR(this, "Failed to save metadata for a file \"%1\"", fileInformation.name);
        return ResultCode::ioError;
    }

    m_fileInformationByName.insert(fileInformation.name, info);

    lock.unlock();

    emit fileAdded(info);

    NX_DEBUG(this, "Add downloaded file (%1) succeeded", info.fullFilePath);
    return ResultCode::ok;
}

ResultCode Storage::addNewFile(const FileInformation& fileInformation, bool loadingDownloads)
{
    NX_ASSERT(fileInformation.status != FileInformation::Status::downloaded);

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!loadingDownloads && m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    if (m_fileInformationByName.contains(fileInformation.name))
        return ResultCode::fileAlreadyExists;

    FileMetadata info = FileMetadata::fromFileInformation(fileInformation, m_downloadsDirectory);
    if (!nx::utils::file_system::ensureDir(info.absoluteDirectoryPath))
    {
        NX_ERROR(this, "Failed to generate folder \"%1\" for a file \"%2\"",
            info.absoluteDirectoryPath, info.name);
        return ResultCode::ioError;
    }

    if (!info.md5.isEmpty() && calculateMd5(info.fullFilePath) == info.md5)
    {
        info.status = FileInformation::Status::downloaded;
        info.size = calculateFileSize(info.fullFilePath);
        if (info.size < 0)
            return ResultCode::ioError;

        const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
        info.downloadedChunks.fill(true, chunkCount);
    }
    else
    {
        if (info.status == FileInformation::Status::notFound)
            info.status = FileInformation::Status::downloading;

        if (info.status == FileInformation::Status::uploading)
        {
            if (info.size < 0)
                return ResultCode::invalidFileSize;

            if (info.md5.isEmpty())
                return ResultCode::invalidChecksum;
        }

        if (info.size >= 0)
        {
            const int chunkCount = calculateChunkCount(info.size, info.chunkSize);
            info.downloadedChunks.resize(chunkCount);
            info.chunkChecksums.clear();
        }

        ResultCode reserveStatus = reserveSpace(info.fullFilePath, info.size >= 0 ? info.size : 0);
        if (reserveStatus != ResultCode::ok)
            return reserveStatus;

        checkDownloadCompleted(info);
    }

    if (!saveMetadata(info))
        return ResultCode::ioError;

    m_fileInformationByName.insert(fileInformation.name, info);

    lock.unlock();

    emit fileAdded(info);

    return ResultCode::ok;
}

ResultCode Storage::updateFileInformation(const QString& fileName, qint64 size,
    const QByteArray& md5)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (it->status == FileInformation::Status::downloaded)
        return ResultCode::fileAlreadyDownloaded;

    bool updated = false;
    bool resizeFailed = false;

    if (size >= 0 && it->size != size)
    {
        it->size = size;

        const int chunkCount = calculateChunkCount(size, it->chunkSize);
        it->downloadedChunks.resize(chunkCount);
        it->chunkChecksums.clear();
        resizeFailed = !QFile::resize(it->fullFilePath, size);

        updated = true;
    }

    if (!md5.isEmpty() && it->md5 != md5)
    {
        it->md5 = md5;
        updated = true;
    }

    const auto exitGuard = nx::utils::makeScopeGuard(
        [this, &lock, updated, it, status = it->status]()
        {
            if (updated || status != it->status)
            {
                lock.unlock();

                emit fileInformationChanged(it.value());
                if (!updated)
                    emit fileStatusChanged(it.value());
            }
        });

    if (resizeFailed)
        return ResultCode::noFreeSpace;

    if (updated)
        checkDownloadCompleted(it.value());

    if (!saveMetadata(it.value()))
        return ResultCode::ioError;

    return ResultCode::ok;
}

ResultCode Storage::setChunkSize(const QString& fileName, qint64 chunkSize)
{
    if (chunkSize <= 0)
        return ResultCode::invalidChunkSize;

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (it->status == FileInformation::Status::downloaded)
        return ResultCode::fileAlreadyDownloaded;

    if (it->chunkSize == chunkSize)
        return ResultCode::ok;

    it->chunkSize = chunkSize;
    if (it->size >= 0)
    {
        const int chunkCount = calculateChunkCount(it->size, chunkSize);
        it->downloadedChunks.fill(false, chunkCount);
        it->chunkChecksums.clear();
    }

    const auto exitGuard = nx::utils::makeScopeGuard(
        [this, &lock, it]()
        {
            lock.unlock();
            emit fileInformationChanged(it.value());
        });

    if (it->status == FileInformation::Status::corrupted)
        it->status = FileInformation::Status::downloading;

    if (!saveMetadata(it.value()))
        return ResultCode::ioError;

    return ResultCode::ok;
}

ResultCode Storage::readFileChunk(const QString& fileName, int chunkIndex, QByteArray& buffer)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size())
        return ResultCode::invalidChunkIndex;

    if (!it->downloadedChunks.testBit(chunkIndex))
        return ResultCode::invalidChunkIndex;

    QFile file(it->fullFilePath);
    if (!file.open(QFile::ReadOnly))
        return ResultCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return ResultCode::ioError;

    const qint64 bytesToRead = std::min(it->chunkSize, it->size - file.pos());
    buffer = file.read(bytesToRead);
    if (buffer.size() != bytesToRead)
        return ResultCode::ioError;

    return ResultCode::ok;
}

ResultCode Storage::writeFileChunk(const QString& fileName, int chunkIndex, const QByteArray& buffer)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (chunkIndex < 0 || chunkIndex >= it->downloadedChunks.size() || it->size < 0)
        return ResultCode::invalidChunkIndex;

    if (it->status == FileInformation::Status::downloaded)
        return ResultCode::fileAlreadyDownloaded;

    QFile file(it->fullFilePath);
    if (!file.open(QFile::ReadWrite)) //< ReadWrite because WriteOnly implies Truncate.
        return ResultCode::ioError;

    if (!file.seek(it->chunkSize * chunkIndex))
        return ResultCode::ioError;

    const qint64 bytesToWrite = calculateChunkSize(it->size, chunkIndex, it->chunkSize);
    if (bytesToWrite < 0)
        return ResultCode::ioError;

    if (bytesToWrite != buffer.size())
        return ResultCode::invalidChunkSize;

    if (file.write(buffer.data(), bytesToWrite) != bytesToWrite)
        return ResultCode::ioError;

    file.close();

    const auto exitGuard = nx::utils::makeScopeGuard(
        [this, &lock, it, status = it->status]()
        {
            lock.unlock();

            emit fileInformationChanged(it.value());

            if (status != it->status)
                emit fileStatusChanged(it.value());
        });

    it->downloadedChunks.setBit(chunkIndex);
    it->touchTime = QDateTime::currentMSecsSinceEpoch();
    checkDownloadCompleted(it.value());
    saveMetadata(it.value());

    return ResultCode::ok;
}

ResultCode Storage::deleteFile(const QString& fileName, bool deleteData)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_loadDownloadsWatcher.isRunning())
            return ResultCode::loadingDownloads;

        const auto resultCode = deleteFileInternal(fileName, deleteData);
        if (resultCode != ResultCode::ok)
            return resultCode;
    }

    emit fileDeleted(filePath(fileName));
    return ResultCode::ok;
}

ResultCode Storage::deleteFileInternal(const QString& fileName, bool deleteData)
{
    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (deleteData && QFile::exists(it->fullFilePath))
    {
        if (!QFile::remove(it->fullFilePath))
            return ResultCode::ioError;
    }

    const auto metadataFileName = this->metadataFileName(it->fullFilePath);
    if (QFile::exists(metadataFileName))
    {
        if (!QFile::remove(metadataFileName))
            return ResultCode::ioError;
    }

    // We won't delete custom user directories.
    if (it->absoluteDirectoryPath.contains(m_downloadsDirectory.absolutePath()))
    {
        QDir dir = QFileInfo(it->absoluteDirectoryPath).absoluteDir();
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
            else
            {
                break;
            }
        }
    }

    m_fileInformationByName.erase(it);

    return ResultCode::ok;
}

ResultCode Storage::clearFile(const QString& fileName, bool force)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    const auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (!force && it->status == FileInformation::Status::downloaded)
        return ResultCode::fileAlreadyDownloaded;

    if (it->status != FileInformation::Status::uploading)
        it->status = FileInformation::Status::downloading;
    it->downloadedChunks.fill(false);

    lock.unlock();
    emit fileInformationChanged(*it);
    emit fileStatusChanged(*it);

    return ResultCode::ok;
}

QVector<QByteArray> Storage::getChunkChecksums(const QString& fileName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto info = fileMetadata(fileName);
    lock.unlock();

    if (!info.isValid())
        return QVector<QByteArray>();

    if (info.chunkChecksums.isEmpty())
    {
        info.chunkChecksums = calculateChecksums(info.fullFilePath, info.chunkSize);

        NX_MUTEX_LOCKER lock(&m_mutex);
        if (auto it = m_fileInformationByName.find(fileName); it != m_fileInformationByName.end())
            it->chunkChecksums = info.chunkChecksums;
    }

    return info.chunkChecksums;
}

ResultCode Storage::setChunkChecksums(
    const QString& fileName, const QVector<QByteArray>& chunkChecksums)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return ResultCode::loadingDownloads;

    auto it = m_fileInformationByName.find(fileName);
    if (it == m_fileInformationByName.end())
        return ResultCode::fileDoesNotExist;

    if (it->chunkChecksums == chunkChecksums)
        return ResultCode::ok;

    if (it->status == FileInformation::Status::downloaded)
        return ResultCode::fileAlreadyDownloaded;

    if (it->size >= 0 && it->downloadedChunks.size() != chunkChecksums.size())
        return ResultCode::invalidChecksum;

    it->chunkChecksums = chunkChecksums;

    const auto exitGuard = nx::utils::makeScopeGuard(
        [this, &lock, it, status = it->status]()
        {
            lock.unlock();

            emit fileInformationChanged(it.value());

            if (status != it->status)
                emit fileStatusChanged(it.value());
        });

    if (it->size < 0)
        return ResultCode::ok;

    auto actualChecksums = calculateChecksums(it->fullFilePath, it->chunkSize);
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

    return ResultCode::ok;
}

void Storage::cleanupExpiredFiles()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    QSet<QString> expiredFiles;
    for (const FileMetadata& data: m_fileInformationByName)
    {
        if (data.ttl > 0 && data.touchTime + data.ttl <= currentTime)
            expiredFiles.insert(data.fullFilePath);
    }

    QSet<QString> successfullyDeletedFiles;
    for (const QString& name: expiredFiles)
    {
        if (deleteFileInternal(name, /*deleteData*/ true) == ResultCode::ok)
            successfullyDeletedFiles.insert(name);
    }

    lock.unlock();

    for (const auto& fileName: successfullyDeletedFiles)
        emit fileDeleted(fileName);
}

void Storage::loadExistingDownloads(bool waitForFinished)
{
    if (!m_downloadsDirectory.exists())
    {
        emit existingDownloadsLoaded();
        return;
    }

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_loadDownloadsWatcher.isRunning())
        return;

    m_loadDownloadsWatcher.setFuture(QtConcurrent::run(
        [this]()
        {
            nx::suppressExceptions(
                [this]() { findDownloadsImpl(); }, this, "at findDownloadsImpl()")();
            nx::suppressExceptions(
                [this]() { cleanupExpiredFiles(); }, this, "at cleanupExpiredFiles()")();
        }));

    if (waitForFinished)
    {
        lock.unlock();
        m_loadDownloadsWatcher.waitForFinished();
    }
}

void Storage::findDownloadsImpl()
{
    const auto startTime = system_clock::now();

    int count = 0;
    for (const auto& entry: QDir(metadataDirectoryPath()).entryInfoList(QDir::Files))
    {
        auto fileName = entry.absoluteFilePath();
        NX_DEBUG(this, "Find downloads: Processing metadata file %1", fileName);
        if (!fileName.endsWith(kMetadataSuffix))
            continue;

        const auto fileInfo = loadMetadata(fileName);
        if (!fileInfo.isValid())
        {
            NX_DEBUG(
                this,
                "Find downloads: Load metadata file (%1) failed", fileName);
            continue;
        }

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (m_fileInformationByName.contains(fileInfo.name))
                continue;
        }

        const auto resultCode = addFileInternal(fileInfo, /*loadingDownloads*/ true);
        NX_DEBUG(
            this,
            "Find downloads: Add file (%1) result = %2", fileInfo.name, resultCode);

        if (resultCode == ResultCode::ok)
            ++count;
    }

    const auto duration = duration_cast<milliseconds>(system_clock::now() - startTime);
    NX_DEBUG(this, "Loaded %1 downloads in %2", count, duration);
}

qint64 Storage::defaultChunkSize()
{
    return kDefaultChunkSize;
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

    return (int) ((fileSize + chunkSize - 1) / chunkSize);
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

    QByteArray data;
    data.resize(chunkSize);

    for (int i = 0; i < chunkCount; ++i)
    {
        const auto size = file.read(data.data(), chunkSize);
        if (size != calculateChunkSize(fileSize, i, chunkSize))
            return {};

        data.resize(size);
        result[i] = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    }

    return result;
}

bool Storage::saveMetadata(const FileMetadata& fileInformation)
{
    if (!nx::utils::file_system::ensureDir(metadataDirectoryPath()))
        return false;

    const auto fileName = metadataFileName(fileInformation.fullFilePath);

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
    file.close();
    const bool deserializeResult = QJson::deserialize(data, &fileInfo);
    NX_DEBUG(
        this,
        "load metadata (%1). Deserialize result: %2. File information valid: %3",
            fileName, deserializeResult ? "success" : "fail", fileInfo.isValid());

    if (!fileInfo.isValid())
    {
        file.remove();
        NX_DEBUG(this, "File metadata removed: %1", fileName);
        return {};
    }

    const auto previousStatus = fileInfo.status;
    checkDownloadCompleted(fileInfo);

    if (previousStatus != fileInfo.status)
        saveMetadata(fileInfo);

    return fileInfo;
}

QString Storage::metadataDirectoryPath() const
{
    return m_downloadsDirectory.absoluteFilePath(kMetadataSuffix);
}

FileMetadata Storage::fileMetadata(const QString& fileName) const
{
    return m_fileInformationByName.value(fileName);
}

ResultCode Storage::loadDownload(const QString& fileName)
{
    auto fileInfo = loadMetadata(fileName);

    if (!fileInfo.isValid())
        return ResultCode::fileDoesNotExist;

    return addFile(fileInfo, /*updateTouchTime*/ false);
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

    const auto md5 = calculateMd5(fileInfo.fullFilePath);
    if (md5 != fileInfo.md5)
    {
        fileInfo.status = FileInformation::Status::corrupted;
        return;
    }

    fileInfo.status = FileInformation::Status::downloaded;
}

ResultCode Storage::reserveSpace(const QString& fileName, const qint64 size)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadWrite))
        return ResultCode::ioError;

    if (!nx::utils::file_system::reserveSpace(file, size))
    {
        file.close();
        file.remove();
        return ResultCode::noFreeSpace;
    }

    return ResultCode::ok;
}

QString Storage::metadataFileName(const QString& fileName)
{
    NX_ASSERT(!fileName.isEmpty());
    if (fileName.isEmpty())
        return QString();

    return QDir(metadataDirectoryPath()).absoluteFilePath(QFileInfo(fileName).fileName() + kMetadataSuffix);
}

qint64 Storage::calculateChunkSize(qint64 fileSize, int chunkIndex, qint64 chunkSize)
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

} // nx::vms::common::p2p::downloader
