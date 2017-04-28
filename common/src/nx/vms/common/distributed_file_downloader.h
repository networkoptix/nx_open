#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QBitArray>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {

struct DownloaderFileInformation
{
    Q_GADGET

public:
    DownloaderFileInformation();
    DownloaderFileInformation(const QString& fileName);

    bool isValid() const;

    enum class Status
    {
        //! There's no such file in the downloader.
        notFound,
        //! The file is downloading from an external URL and system servers.
        downloading,
        //! Waiting for the file to be manually uploaded.
        uploading,
        //! The file is downloaded.
        downloaded,
        //! The file has been downloaded, but checksum validation has failed.
        corrupted
    };
    Q_ENUM(Status)

    QString name;
    qint64 size = -1;
    QByteArray md5;
    QUrl url;
    qint64 chunkSize = 0;
    Status status = Status::notFound;
    QBitArray downloadedChunks;
};
#define DownloaderFileInformation_Fields \
    (name)(size)(md5)(url)(chunkSize)(status)(downloadedChunks)

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
        fileAlreadyDownloaded,
        invalidChecksum,
        invalidFileSize,
        invalidChunkIndex,
        invalidChunkSize,
        noFreeSpace
    };
    Q_ENUM(ErrorCode)

    DistributedFileDownloader(const QDir& downloadsDirectory, QObject* parent = nullptr);

    ~DistributedFileDownloader();

    QStringList files() const;

    QString filePath(const QString& fileName) const;

    DownloaderFileInformation fileInformation(const QString& fileName) const;

    ErrorCode addFile(const DownloaderFileInformation& fileInformation);

    ErrorCode updateFileInformation(
        const QString& fileName,
        int size,
        const QByteArray& md5);

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

private:
    QScopedPointer<DistributedFileDownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DistributedFileDownloader)
};

QN_FUSION_DECLARE_FUNCTIONS(DownloaderFileInformation, (json))
QN_FUSION_DECLARE_FUNCTIONS(DownloaderFileInformation::Status, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(DistributedFileDownloader::ErrorCode, (lexical))

} // namespace common
} // namespace vms
} // namespace nx
