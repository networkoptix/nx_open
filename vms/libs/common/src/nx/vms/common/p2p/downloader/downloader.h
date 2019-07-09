#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <common/common_module_aware.h>

#include "result_code.h"
#include "file_information.h"

namespace nx::vms::common::p2p::downloader {

class AbstractPeerManager;

class Downloader: public QObject, public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT

public:
    Downloader(
        const QDir& downloadsDirectory,
        QnCommonModule* commonModule,
        const QList<AbstractPeerManager*>& peerManagers = {},
        QObject* parent = nullptr);

    virtual ~Downloader() override;

    QStringList files() const;

    QString filePath(const QString& fileName) const;

    QDir downloadsDirectory() const;

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

    void startDownloads();
    void stopDownloads();
    void findExistingDownloads();

    bool isStalled(const QString& fileName) const;

signals:
    void downloadFinished(const QString& fileName);
    void downloadFailed(const QString& fileName);
    void downloadStalledChanged(const QString& fileName, bool stalled);
    void fileAdded(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileDeleted(const QString& fileName);
    void fileInformationChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void fileStatusChanged(
        const nx::vms::common::p2p::downloader::FileInformation& fileInformation);
    void chunkDownloadFailed(const QString& fileName);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // nx::vms::common::p2p::downloader
