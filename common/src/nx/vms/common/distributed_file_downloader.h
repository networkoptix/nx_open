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
        notFound,
        downloading,
        downloaded,
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

QN_FUSION_DECLARE_FUNCTIONS(DownloaderFileInformation::Status, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(DownloaderFileInformation, (json))

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

    DistributedFileDownloader(const QDir& downloadsDirectory, QObject* parent = nullptr);
    ~DistributedFileDownloader();

    QStringList files() const;

    QString filePath(const QString& fileName) const;

    DownloaderFileInformation fileInformation(const QString& fileName) const;

    ErrorCode addFile(const DownloaderFileInformation& fileInformation);

    ErrorCode readFileChunk(
        const QString& fileName,
        int chunkIndex,
        QByteArray& buffer);

    ErrorCode writeFileChunk(
        const QString& fileName,
        int chunkIndex,
        const QByteArray& buffer);

    ErrorCode deleteFile(const QString& fileName, bool deleteData = true);

    QVector<QByteArray> getChunkChecksums(const QString& filePath);

    static QByteArray calculateMd5(const QString& filePath);
    static qint64 calculateFileSize(const QString& filePath);
    static int calculateChunkCount(qint64 fileSize, qint64 chunkSize);

private:
    QScopedPointer<DistributedFileDownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DistributedFileDownloader)
};

} // namespace common
} // namespace vms
} // namespace nx
