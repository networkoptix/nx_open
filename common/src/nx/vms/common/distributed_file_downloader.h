#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QBitArray>
#include <QtCore/QVector>
#include <QtCore/QDir>

namespace nx {
namespace vms {
namespace common {

class DistributedFileDownloaderPrivate;
class DistributedFileDownloader: public QObject
{
    Q_OBJECT

public:
    enum class ErrorCode
    {
        noError,
        ioError,
        fileDoesNotExist,
        fileAlreadyExists,
        invalidChecksum,
        invalidFileSize,
        invalidChunkIndex,
        invalidChunkSize,
        noFreeSpace
    };

    enum class FileStatus
    {
        notFound,
        downloading,
        downloaded,
        corrupted
    };
    Q_ENUM(FileStatus)

    struct FileInformation
    {
        FileInformation();
        FileInformation(const QString& fileName);

        QString name;
        qint64 size = -1;
        QByteArray md5;
        QUrl url;
        qint64 chunkSize = 0;
        FileStatus status = FileStatus::notFound;
        QBitArray downloadedChunks;
    };

    DistributedFileDownloader(const QDir& downloadsDirectory, QObject* parent = nullptr);
    ~DistributedFileDownloader();

    QStringList files() const;

    FileInformation fileInformation(const QString& fileName) const;

    ErrorCode addFile(const FileInformation& fileInformation);

    ErrorCode readFileChunk(
        const QString& fileName,
        int chunkIndex,
        QByteArray& buffer);

    ErrorCode writeFileChunk(
        const QString& fileName,
        int chunkIndex,
        const QByteArray& buffer);

    ErrorCode deleteFile(const QString& fileName, bool deleteData = true);

    QVector<QByteArray> getChunkChecksums(const QString& fileName);

    static QByteArray calculateMd5(const QString& fileName);
    static qint64 calculateFileSize(const QString& fileName);
    static int calculateChunkCount(qint64 fileSize, qint64 chunkSize);

private:
    QScopedPointer<DistributedFileDownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DistributedFileDownloader)
};

} // namespace common
} // namespace vms
} // namespace nx
