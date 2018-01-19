#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <common/common_module_aware.h>

#include "result_code.h"
#include "file_information.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

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

    ResultCode addFile(const FileInformation& fileInformation);

    ResultCode updateFileInformation(
        const QString& fileName,
        int size,
        const QByteArray& md5);

    ResultCode readFileChunk(
        const QString& fileName,
        int chunkIndex,
        QByteArray& buffer);

    ResultCode writeFileChunk(
        const QString& fileName,
        int chunkIndex,
        const QByteArray& buffer);

    ResultCode deleteFile(const QString& fileName, bool deleteData = true);

    QVector<QByteArray> getChunkChecksums(const QString& fileName);
    void atServerStart();

signals:
    void downloadFinished(const QString& fileName);
    void downloadFailed(const QString& fileName);
    void fileAdded(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileDeleted(const QString& fileName);
    void fileInformationChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileStatusChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);

private:
    QScopedPointer<DownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(Downloader)
};

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
