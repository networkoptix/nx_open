#ifndef DOWNLOAD_UPDATES_PEER_TASK_H
#define DOWNLOAD_UPDATES_PEER_TASK_H

#include <QtCore/QDir>

#include <update/task/network_peer_task.h>

class QNetworkAccessManager;

class QnDownloadUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        DownloadError,
        NoFreeSpaceError,
        FileError
    };

    explicit QnDownloadUpdatesPeerTask(QObject *parent = 0);

    void setTargets(const QHash<QUrl, QString> &resultingFiles);
    void setHashes(const QHash<QUrl, QString> &hashByUrl);
    void setFileSizes(const QHash<QUrl, qint64> &fileSizeByUrl);
    QHash<QUrl, QString> resultingFiles() const;
    void setPeerAssociations(const QMultiHash<QUrl, QnUuid> &peersByUrl);

protected:
    virtual void doStart() override;
    virtual void doCancel() override;

private:
    void downloadNextUpdate();

private slots:
    void continueDownload();
    void at_downloadReply_finished();
    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void at_downloadReply_readyRead();

private:
    QHash<QUrl, QString> m_targets;
    QHash<QUrl, QString> m_hashByUrl;
    QMultiHash<QUrl, QnUuid> m_peersByUrl;
    QHash<QUrl, QString> m_resultingFiles;
    QHash<QUrl, qint64> m_fileSizeByUrl;

    QList<QUrl> m_pendingDownloads;
    QSet<QnUuid> m_currentPeers;
    int m_triesCount;

    QScopedPointer<QFile> m_file;
    QNetworkAccessManager *m_networkAccessManager;
};

#endif // DOWNLOAD_UPDATES_PEER_TASK_H
