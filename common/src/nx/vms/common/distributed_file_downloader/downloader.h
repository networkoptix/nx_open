#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <common/common_module_aware.h>

#include "error_code.h"
#include "file_information.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class AbstractPeerManagerFactory;

class DownloaderPrivate;
class Downloader: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
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

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
