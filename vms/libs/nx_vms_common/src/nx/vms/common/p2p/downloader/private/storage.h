// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QDir>
#include <QtCore/QFutureWatcher>

#include <nx/utils/thread/mutex.h>

#include "../result_code.h"
#include "../file_information.h"

namespace nx::vms::common::p2p::downloader {

struct FileMetadata: FileInformation
{
    FileMetadata()
    {
    }

    FileMetadata(const FileInformation& fileInformation):
        FileInformation(fileInformation)
    {
    }

    static FileMetadata fromFileInformation(
        const FileInformation& fileInformation,
        const QDir& defaultDownloadsDirectory);

    QVector<QByteArray> chunkChecksums;
};

class NX_VMS_COMMON_API Storage: public QObject
{
    Q_OBJECT

public:
    Storage(const QDir& m_downloadsDirectory, QObject* parent = nullptr);
    virtual ~Storage() override;

    QDir downloadsDirectory() const;

    QStringList files() const;
    FileInformation fileInformation(const QString& fileName) const;

    ResultCode addFile(FileInformation fileInformation, bool updateTouchTime = true);
    ResultCode moveFile(
        const QString& fileName, const QString& folderPath, const QString& type = QString());
    ResultCode updateFileInformation(const QString& fileName, qint64 size, const QByteArray& md5);
    ResultCode setChunkSize(const QString& fileName, qint64 chunkSize);

    ResultCode readFileChunk(const QString& fileName, int chunkIndex, QByteArray& buffer);
    ResultCode writeFileChunk(const QString& fileName, int chunkIndex, const QByteArray& buffer);

    ResultCode deleteFile(const QString& fileName, bool deleteData = true);
    ResultCode clearFile(const QString& fileName, bool force = false);

    QVector<QByteArray> getChunkChecksums(const QString& fileName);
    ResultCode setChunkChecksums(
        const QString& fileName, const QVector<QByteArray>& chunkChecksums);

    void cleanupExpiredFiles();

    void loadExistingDownloads(bool waitForFinished = false);
    QString filePath(const QString& fileName) const;

    static qint64 defaultChunkSize();
    static QByteArray calculateMd5(const QString& filePath);
    static qint64 calculateFileSize(const QString& filePath);
    static int calculateChunkCount(qint64 fileSize, qint64 calculateChunkSize);
    static QVector<QByteArray> calculateChecksums(const QString& filePath, qint64 chunkSize);

signals:
    void fileAdded(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileDeleted(const QString& fileName);
    void fileInformationChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileStatusChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);

    void existingDownloadsLoaded();

private:
    ResultCode addFileInternal(FileInformation fileInformation, bool loadingDownloads = false);
    ResultCode addDownloadedFile(
        const FileInformation& fileInformation, bool loadingDownloads = false);
    ResultCode addNewFile(const FileInformation& fileInformation, bool loadingDownloads = false);
    ResultCode deleteFileInternal(const QString& fileName, bool deleteData = true);

    bool saveMetadata(const FileMetadata& fileInformation);
    FileMetadata loadMetadata(const QString& fileName);
    FileMetadata fileMetadata(const QString& fileName) const;
    ResultCode loadDownload(const QString& fileName);
    void checkDownloadCompleted(FileMetadata& fileInfo);
    void findDownloadsImpl();
    QString metadataDirectoryPath() const;
    QString metadataFileName(const QString& fileName);

    static ResultCode reserveSpace(const QString& fileName, const qint64 size);
    static qint64 calculateChunkSize(qint64 fileSize, int chunkIndex, qint64 calculateChunkSize);

private:
    const QDir m_downloadsDirectory;
    QHash<QString, FileMetadata> m_fileInformationByName;
    QFutureWatcher<void> m_loadDownloadsWatcher;

    mutable nx::Mutex m_mutex;
};

} // nx::vms::common::p2p::downloader
