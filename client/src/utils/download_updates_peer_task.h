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
    void setTargets(const QHash<QUrl, QString> &targets);
    void setPeerAssociations(const QMultiHash<QUrl, QnId> &peersByUrl);

protected:
    virtual void doStart() override;

private:
    void downloadNextUpdate();

private slots:
    void at_downloadReply_finished();
    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void at_downloadReply_readyRead();

private:
    QDir m_targetDirPath;
    QHash<QUrl, QString> m_targets;
    QMultiHash<QUrl, QnId> m_peersByUrl;

    QUrl m_currentUrl;
    QSet<QnId> m_currentPeers;

    int m_allTargets;
    QScopedPointer<QFile> m_file;
    QNetworkAccessManager *m_networkAccessManager;
};

#endif // DOWNLOAD_UPDATES_PEER_TASK_H
