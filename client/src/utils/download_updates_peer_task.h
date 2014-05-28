#ifndef DOWNLOAD_UPDATES_PEER_TASK_H
#define DOWNLOAD_UPDATES_PEER_TASK_H

#include <QtCore/QDir>

#include <utils/network_peer_task.h>

class QNetworkAccessManager;

class QnDownloadUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        DownloadError,
        FileError
    };

    explicit QnDownloadUpdatesPeerTask(QObject *parent = 0);

    void setTargetDir(const QString &path);
    void setTargets(const QHash<QUrl, QString> &resultingFiles);
    QHash<QUrl, QString> resultingFiles() const;
    void setPeerAssociations(const QMultiHash<QUrl, QnId> &peersByUrl);

    virtual void cancel() override;

protected:
    virtual void doStart() override;

private:
    void downloadNextUpdate();

private slots:
    void at_downloadReply_finished();
    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void at_downloadReply_readyRead();

private:
    QString m_targetDirPath;
    QHash<QUrl, QString> m_targets;
    QMultiHash<QUrl, QnId> m_peersByUrl;
    QHash<QUrl, QString> m_resultingFiles;

    QList<QUrl> m_pendingDownloads;
    QSet<QnId> m_currentPeers;

    QScopedPointer<QFile> m_file;
    QNetworkAccessManager *m_networkAccessManager;
};

#endif // DOWNLOAD_UPDATES_PEER_TASK_H
