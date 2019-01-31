#ifndef DOWNLOAD_UPDATES_PEER_TASK_H
#define DOWNLOAD_UPDATES_PEER_TASK_H

#include <QtCore/QDir>

#include <update/task/network_peer_task.h>
#include <nx/utils/url.h>

class QNetworkAccessManager;

class QnDownloadUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        DownloadError,
        NoFreeSpaceError,
        FileError,
        Cancelled
    };

    explicit QnDownloadUpdatesPeerTask(QObject *parent = 0);

    void setTargets(const QHash<nx::utils::Url, QString> &resultingFiles);
    void setHashes(const QHash<nx::utils::Url, QString> &hashByUrl);
    void setFileSizes(const QHash<nx::utils::Url, qint64> &fileSizeByUrl);
    QHash<nx::utils::Url, QString> resultingFiles() const;
    void setPeerAssociations(const QMultiHash<nx::utils::Url, QnUuid> &peersByUrl);

protected:
    virtual void doStart() override;
    virtual void doCancel() override;

private:
    void downloadNextUpdate();

    void finishTask(ErrorCode code);

private slots:
    void continueDownload();
    void at_downloadReply_finished();
    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void at_downloadReply_readyRead();

private:
    QHash<nx::utils::Url, QString> m_targets;
    QHash<nx::utils::Url, QString> m_hashByUrl;
    QMultiHash<nx::utils::Url, QnUuid> m_peersByUrl;
    QHash<nx::utils::Url, QString> m_resultingFiles;
    QHash<nx::utils::Url, qint64> m_fileSizeByUrl;

    QList<nx::utils::Url> m_pendingDownloads;
    QSet<QnUuid> m_currentPeers;
    int m_triesCount;

    QScopedPointer<QFile> m_file;
    QNetworkAccessManager *m_networkAccessManager;
};

#endif // DOWNLOAD_UPDATES_PEER_TASK_H
