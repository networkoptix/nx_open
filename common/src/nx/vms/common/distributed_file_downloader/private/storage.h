#pragma once

#include <QtCore/QHash>
#include <QtCore/QDir>

#include <nx/utils/thread/mutex.h>

#include "../error_code.h"
#include "../file_information.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

struct FileMetadata: FileInformation
{
    FileMetadata()
    {
    }

    FileMetadata(const FileInformation& fileInformation):
        FileInformation(fileInformation)
    {
    }

    QVector<QByteArray> chunkChecksums;
};

class Storage
{
public:
    Storage(const QDir& m_downloadsDirectory);

    QDir downloadsDirectory() const;

    QStringList files() const;

    QString filePath(const QString& fileName) const;

    FileInformation fileInformation(const QString& fileName) const;

    ErrorCode addFile(const FileInformation& fileInfo);
    ErrorCode updateFileInformation(const QString& fileName, qint64 size, const QByteArray& md5);
    ErrorCode setChunkSize(const QString& fileName, qint64 chunkSize);

    ErrorCode readFileChunk(const QString& fileName, int chunkIndex, QByteArray& buffer);
    ErrorCode writeFileChunk(const QString& fileName, int chunkIndex, const QByteArray& buffer);

    ErrorCode deleteFile(const QString& fileName, bool deleteData = true);

    QVector<QByteArray> getChunkChecksums(const QString& fileName);
    ErrorCode setChunkChecksums(
        const QString& fileName, const QVector<QByteArray>& chunkChecksums);

    void findDownloads();

    static qint64 defaultChunkSize();
    static QByteArray calculateMd5(const QString& filePath);
    static qint64 calculateFileSize(const QString& filePath);
    static int calculateChunkCount(qint64 fileSize, qint64 calculateChunkSize);
    static QVector<QByteArray> calculateChecksums(const QString& filePath, qint64 chunkSize);

private:
    bool saveMetadata(const FileMetadata& fileInformation);
    FileMetadata loadMetadata(const QString& fileName);
    FileMetadata fileMetadata(const QString& fileName) const;
    ErrorCode loadDownload(const QString& fileName);
    void checkDownloadCompleted(FileMetadata& fileInfo);

    static bool reserveSpace(const QString& fileName, const qint64 size);
    static QString metadataFileName(const QString& fileName);
    static qint64 calculateChunkSize(qint64 fileSize, int chunkIndex, qint64 calculateChunkSize);

private:
    const QDir m_downloadsDirectory;
    QHash<QString, FileMetadata> m_fileInformationByName;

    mutable QnMutex m_mutex;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
