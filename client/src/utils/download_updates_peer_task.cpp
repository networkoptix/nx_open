#include "download_updates_peer_task.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace {

    void readAllData(QIODevice *inDevice, QIODevice *outDevice) {
        const int bufferSize = 512 * 1024;
        QByteArray buffer;
        buffer.reserve(bufferSize);

        while (inDevice->bytesAvailable()) {
            int read = inDevice->read(buffer.data(), bufferSize);
            if (read > 0)
                outDevice->write(buffer.data(), read);
        }
    }

    QString updateFilePath(const QString &updatesDirPath, const QString &fileName) {
        QDir dir = QDir::temp();
        if (!dir.exists(updatesDirPath))
            dir.mkdir(updatesDirPath);
        dir.cd(updatesDirPath);
        return dir.absoluteFilePath(fileName);
    }

} // anonymous namespace

QnDownloadUpdatesPeerTask::QnDownloadUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void QnDownloadUpdatesPeerTask::setTargetDir(const QString &path) {
    m_targetDirPath = path;
}

void QnDownloadUpdatesPeerTask::setTargets(const QHash<QUrl, QString> &targets) {
    m_targets = targets;
    m_allTargets = targets.size();
}

void QnDownloadUpdatesPeerTask::setPeerAssociations(const QMultiHash<QUrl, QnId> &peersByUrl) {
    m_peersByUrl = peersByUrl;
}

void QnDownloadUpdatesPeerTask::doStart() {
    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::downloadNextUpdate() {
    if (m_targets.isEmpty()) {
        finish(NoError);
        return;
    }

    auto it = m_targets.begin();
    m_currentUrl = it.key();
    m_currentPeers = QnIdSet::fromList(m_peersByUrl.values(m_currentUrl));

    m_file.reset(new QFile(updateFilePath(m_targetDirPath, *it)));
    if (!m_file->open(QFile::WriteOnly)) {
        finish(FileError);
        return;
    }

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(m_currentUrl));
    connect(reply,  &QNetworkReply::readyRead,          this,   &QnDownloadUpdatesPeerTask::at_downloadReply_readyRead);
    connect(reply,  &QNetworkReply::finished,           this,   &QnDownloadUpdatesPeerTask::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress);
}

void QnDownloadUpdatesPeerTask::at_downloadReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_file.reset();
        finish(DownloadError);
        return;
    }

    readAllData(reply, m_file.data());
    m_file->close();
    m_file.reset();

    foreach (const QnId &peerId, m_currentPeers)
        emit peerFinished(peerId);

    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    foreach (const QnId &peerId, m_currentPeers)
        emit peerProgressChanged(peerId, bytesReceived * 100 / bytesTotal);

    int finished = m_allTargets - m_targets.size();
    emit progressChanged((finished * 100 + bytesReceived * 100 / bytesTotal) / m_allTargets);
}

void QnDownloadUpdatesPeerTask::at_downloadReply_readyRead() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    readAllData(reply, m_file.data());
}
