#include "download_updates_peer_task.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <utils/update/update_utils.h>

namespace {

    const int maxDownloadTries = 20;

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
}

void QnDownloadUpdatesPeerTask::setHashes(const QHash<QUrl, QString> &hashByUrl) {
    m_hashByUrl = hashByUrl;
}

void QnDownloadUpdatesPeerTask::setFileSizes(const QHash<QUrl, qint64> &fileSizeByUrl) {
    m_fileSizeByUrl = fileSizeByUrl;
}

QHash<QUrl, QString> QnDownloadUpdatesPeerTask::resultingFiles() const {
    return m_resultingFiles;
}

void QnDownloadUpdatesPeerTask::setPeerAssociations(const QMultiHash<QUrl, QnId> &peersByUrl) {
    m_peersByUrl = peersByUrl;
}

void QnDownloadUpdatesPeerTask::cancel() {
    m_currentPeers.clear();
    m_pendingDownloads.clear();
    m_file.reset();
}

void QnDownloadUpdatesPeerTask::doStart() {
    m_resultingFiles.clear();
    m_pendingDownloads = m_targets.keys();
    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::downloadNextUpdate() {
    if (m_pendingDownloads.isEmpty()) {
        finish(NoError);
        return;
    }

    QUrl url = m_pendingDownloads.first();
    m_currentPeers = QnIdSet::fromList(m_peersByUrl.values(url));

    QString fileName = updateFilePath(m_targetDirPath, m_targets[url]);
    m_resultingFiles.insert(url, fileName);

    m_file.reset(new QFile(fileName));
    if (!m_file->open(QFile::WriteOnly | QFile::Truncate)) {
        finish(FileError);
        return;
    }

    m_triesCount = 0;
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply,  &QNetworkReply::readyRead,          this,   &QnDownloadUpdatesPeerTask::at_downloadReply_readyRead);
    connect(reply,  &QNetworkReply::finished,           this,   &QnDownloadUpdatesPeerTask::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress);
}

void QnDownloadUpdatesPeerTask::continueDownload() {
    if (m_file.isNull())
        return;

    QUrl url = m_pendingDownloads.first();
    qint64 pos = m_file->pos();

    m_triesCount++;

    QNetworkRequest request(url);
    request.setRawHeader("Range", QString(lit("bytes=%1-")).arg(pos).toLatin1());
    QNetworkReply *reply = m_networkAccessManager->get(request);
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

    // it means the task was cancelled
    if (m_file.isNull())
        return;

    if (reply->error() != QNetworkReply::NoError) {
        if (m_triesCount < maxDownloadTries) {
            continueDownload();
            return;
        }

        m_file.reset();
        m_file->remove();
        finish(DownloadError);
        return;
    }

    readAllData(reply, m_file.data());

    m_file->close();

    QString md5 = makeMd5(m_file.data());

    m_file.reset();

    if (md5 != m_hashByUrl[m_pendingDownloads.first()]) {
        finish(DownloadError);
        return;
    }

    foreach (const QnId &peerId, m_currentPeers)
        emit peerFinished(peerId);

    m_pendingDownloads.removeFirst();
    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    // it means the task was cancelled
    if (m_file.isNull()) {
        reply->deleteLater();
        return;
    }

    if (m_fileSizeByUrl.contains(m_pendingDownloads.first())) {
        bytesReceived = m_file->pos() - 1;
        bytesTotal = m_fileSizeByUrl[m_pendingDownloads.first()];
    } else {
        m_fileSizeByUrl.insert(m_pendingDownloads.first(), bytesTotal);
    }

    foreach (const QnId &peerId, m_currentPeers)
        emit peerProgressChanged(peerId, bytesReceived * 100 / bytesTotal);

    int finished = m_targets.size() - m_pendingDownloads.size();
    emit progressChanged((finished * 100 + bytesReceived * 100 / bytesTotal) / m_targets.size());
}

void QnDownloadUpdatesPeerTask::at_downloadReply_readyRead() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    m_triesCount = 0;

    // it means the task was cancelled
    if (m_file.isNull()) {
        reply->deleteLater();
        return;
    }

    readAllData(reply, m_file.data());

    static int br = 0;

    if (br > m_file->pos())
        br = 0;

    if (m_file->pos() <= 33200686) {
        if (reply->isRunning() && m_file->pos() - br > 3000000) {
            br = m_file->pos();
            reply->abort();
        }
    }
}
