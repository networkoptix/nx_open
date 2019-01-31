#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QDir>

#include <common/common_module_aware.h>

#include "result_code.h"
#include "file_information.h"

namespace nx::vms::common::p2p::downloader {

class AbstractPeerManagerFactory;

class Downloader: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    Downloader(
        const QDir& downloadsDirectory,
        QnCommonModule* commonModule,
        AbstractPeerManagerFactory* peerManagerFactory = nullptr,
        QObject* parent = nullptr);

    virtual ~Downloader() override;

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

    void startDownloads();
    void stopDownloads();

    static void validateAsync(const QString& url, bool onlyConnectionCheck, int expectedSize,
        std::function<void(bool)> callback);

    static bool validate(const QString& url, bool onlyConnectionCheck, int expectedSize);

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
    class Private;
    const QScopedPointer<Private> d;
};

} // nx::vms::common::p2p::downloader
