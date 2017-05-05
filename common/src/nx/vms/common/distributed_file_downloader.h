#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QBitArray>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_module_aware.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class AbstractPeerManagerFactory;

struct FileInformation
{
    Q_GADGET

public:
    FileInformation();
    FileInformation(const QString& fileName);

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
#define FileInformation_Fields \
    (name)(size)(md5)(url)(chunkSize)(status)(downloadedChunks)

class DownloaderPrivate;
class Downloader: public QObject, public QnCommonModuleAware
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

    Downloader(
        const QDir& downloadsDirectory,
        QnCommonModule* commonModule,
        AbstractPeerManagerFactory* peerManagerFactory = nullptr,
        QObject* parent = nullptr);

    ~Downloader();

    QStringList files() const;

    QString filePath(const QString& fileName) const;

    FileInformation fileInformation(const QString& fileName) const;

    ErrorCode addFile(const FileInformation& fileInformation);

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
    QScopedPointer<DownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(Downloader)
};

QN_FUSION_DECLARE_FUNCTIONS(FileInformation, (json))
QN_FUSION_DECLARE_FUNCTIONS(FileInformation::Status, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(Downloader::ErrorCode, (lexical))

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
