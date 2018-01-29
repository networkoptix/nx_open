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

class AbstractDownloader: public QObject
{
    Q_OBJECT

public:
    AbstractDownloader(QObject* parent);
    virtual ~AbstractDownloader() = default;

    virtual QStringList files() const = 0;
    virtual QString filePath(const QString& fileName) const = 0;
    virtual FileInformation fileInformation(const QString& fileName) const = 0;
    virtual ResultCode addFile(const FileInformation& fileInformation) = 0;
    virtual ResultCode updateFileInformation(
        const QString& fileName, int size, const QByteArray& md5) = 0;
    virtual ResultCode readFileChunk(
        const QString& fileName, int chunkIndex, QByteArray& buffer) = 0;
    virtual ResultCode writeFileChunk(
        const QString& fileName, int chunkIndex, const QByteArray& buffer) = 0;
    virtual ResultCode deleteFile(const QString& fileName, bool deleteData = true) = 0;
    virtual QVector<QByteArray> getChunkChecksums(const QString& fileName) = 0;
};

class DownloaderPrivate;
class Downloader: public AbstractDownloader, public QnCommonModuleAware
{
    Q_OBJECT

public:
    Downloader(
        const QDir& downloadsDirectory,
        QnCommonModule* commonModule,
        AbstractPeerManagerFactory* peerManagerFactory = nullptr,
        QObject* parent = nullptr);

    ~Downloader();

    virtual QStringList files() const override;

    virtual QString filePath(const QString& fileName) const override;

    virtual FileInformation fileInformation(const QString& fileName) const override;

    virtual ResultCode addFile(const FileInformation& fileInformation) override;

    virtual ResultCode updateFileInformation(
        const QString& fileName,
        int size,
        const QByteArray& md5) override;

    virtual ResultCode readFileChunk(
        const QString& fileName,
        int chunkIndex,
        QByteArray& buffer) override;

    virtual ResultCode writeFileChunk(
        const QString& fileName,
        int chunkIndex,
        const QByteArray& buffer) override;

    virtual ResultCode deleteFile(const QString& fileName, bool deleteData = true) override;

    virtual QVector<QByteArray> getChunkChecksums(const QString& fileName) override;
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
    void chunkDownloadFailed(const QString& fileName);

private:
    QScopedPointer<DownloaderPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(Downloader)
};

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
