#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QBitArray>

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
        invalidChecksum
    };

    enum class FileStatus
    {
        notFound,
        downloading,
        downloaded
    };

    struct FileInformation
    {
        FileInformation();
        FileInformation(const QString& fileName);

        QString name;
        qint64 size = 0;
        QByteArray md5;
        QUrl url;
        qint64 chunkSize = 0;
        FileStatus status = FileStatus::notFound;
        QBitArray downloadedChunks;
    };

    DistributedFileDownloader(QObject* parent = nullptr);
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
        const QByteArray& data);

    ErrorCode deleteFile(const QString& fileName, bool deleteData = true);

    ErrorCode findDownloads(const QString& path);

private:
    QScopedPointer<DistributedFileDownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DistributedFileDownloader)
};

} // namespace common
} // namespace vms
} // namespace nx
