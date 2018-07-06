#include "download_updates_peer_task.h"

#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <utils/update/update_utils.h>
#include <nx/utils/log/log.h>

namespace {

    const int maxDownloadTries = 20;

    bool readAllData(QIODevice *inDevice, QIODevice *outDevice) {
        const int bufferSize = 512 * 1024;
        QByteArray buffer;
        buffer.reserve(bufferSize);

        while (inDevice->bytesAvailable()) {
            int read = inDevice->read(buffer.data(), bufferSize);
            if (read > 0)
                return outDevice->write(buffer.data(), read) == read;
        }

        return true;
    }

} // anonymous namespace

QnDownloadUpdatesPeerTask::QnDownloadUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void QnDownloadUpdatesPeerTask::setTargets(const QHash<nx::utils::Url, QString> &targets) {
    m_targets = targets;
}

void QnDownloadUpdatesPeerTask::setHashes(const QHash<nx::utils::Url, QString> &hashByUrl) {
    m_hashByUrl = hashByUrl;
}

void QnDownloadUpdatesPeerTask::setFileSizes(const QHash<nx::utils::Url, qint64> &fileSizeByUrl) {
    m_fileSizeByUrl = fileSizeByUrl;
}

QHash<nx::utils::Url, QString> QnDownloadUpdatesPeerTask::resultingFiles() const {
    return m_resultingFiles;
}

void QnDownloadUpdatesPeerTask::setPeerAssociations(const QMultiHash<nx::utils::Url, QnUuid> &peersByUrl) {
    m_peersByUrl = peersByUrl;
}

void QnDownloadUpdatesPeerTask::finishTask(ErrorCode code)
{
    m_currentPeers.clear();
    m_pendingDownloads.clear();
    m_file.reset();
    finish(code);
}

void QnDownloadUpdatesPeerTask::doCancel()
{
    finishTask(Cancelled);
}

void QnDownloadUpdatesPeerTask::doStart() {
    NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Starting download %1 file(s).").arg(m_targets.size()), cl_logDEBUG1);

    m_resultingFiles.clear();
    m_pendingDownloads = m_targets.keys();
    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::downloadNextUpdate() {
    if (m_pendingDownloads.isEmpty()) {
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Download finished."), cl_logDEBUG1);
        finish(NoError);
        return;
    }

    nx::utils::Url url = m_pendingDownloads.first();
    m_currentPeers = QSet<QnUuid>::fromList(m_peersByUrl.values(url));

    QString fileName = updateFilePath(m_targets[url]);
    m_resultingFiles.insert(url, fileName);

    NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Starting download [%1 -> %2].").arg(url.toString(QUrl::RemovePassword)).arg(fileName), cl_logDEBUG1);

    m_file.reset(new QFile(fileName));
    if (!m_file->open(QFile::WriteOnly | QFile::Truncate)) {
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Could not open file %1.").arg(fileName), cl_logERROR);
        finish(FileError);
        return;
    }

    m_triesCount = 0;
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url.toQUrl()));
    connect(reply,  &QNetworkReply::readyRead,          this,   &QnDownloadUpdatesPeerTask::at_downloadReply_readyRead);
    connect(reply,  &QNetworkReply::finished,           this,   &QnDownloadUpdatesPeerTask::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress);
}

void QnDownloadUpdatesPeerTask::continueDownload() {
    if (m_file.isNull())
        return;

    nx::utils::Url url = m_pendingDownloads.first();
    qint64 pos = m_file->pos();

    m_triesCount++;

    QNetworkRequest request(url.toQUrl());
    request.setRawHeader("Range", QString(lit("bytes=%1-")).arg(pos).toLatin1());
    NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Continue download at %1.").arg(pos), cl_logDEBUG2);
    QNetworkReply *reply = m_networkAccessManager->get(request);
    connect(reply,  &QNetworkReply::readyRead,          this,   &QnDownloadUpdatesPeerTask::at_downloadReply_readyRead);
    connect(reply,  &QNetworkReply::finished,           this,   &QnDownloadUpdatesPeerTask::at_downloadReply_finished);
    connect(reply,  &QNetworkReply::downloadProgress,   this,   &QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress);
}

void QnDownloadUpdatesPeerTask::at_downloadReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        NX_ASSERT(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    // it means the task was cancelled
    if (m_file.isNull())
        return;

    if (reply->error() != QNetworkReply::NoError) {
        switch (reply->error()) {
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::TimeoutError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
            if (m_triesCount < maxDownloadTries) {
                QTimer::singleShot(m_triesCount * 50, this, SLOT(continueDownload()));
                return;
            }
            break;
        default:
            break;
        }

        m_file->remove();
        m_file.reset();
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Network error."), cl_logERROR);
        finish(DownloadError);
        return;
    }

    if (!readAllData(reply, m_file.data()))
    {
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: No free space."), cl_logERROR);
        finishTask(NoFreeSpaceError);
        return;
    }

    m_file->close();

    QString md5 = makeMd5(m_file.data());

    m_file.reset();

    if (md5 != m_hashByUrl[m_pendingDownloads.first()]) {
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Checksum check is failed."), cl_logERROR);
        finish(DownloadError);
        return;
    }

    foreach (const QnUuid &peerId, m_currentPeers)
        emit peerFinished(peerId);

    NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Download finished [%1].").arg(m_pendingDownloads.first().toString()), cl_logDEBUG1);

    m_pendingDownloads.removeFirst();
    downloadNextUpdate();
}

void QnDownloadUpdatesPeerTask::at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        NX_ASSERT(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
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

    if (bytesReceived > bytesTotal) {
        m_file->close();
        m_file->remove();
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: Wrong file size."), cl_logERROR);
        finishTask(DownloadError);
        return;
    }

    foreach (const QnUuid &peerId, m_currentPeers)
        emit peerProgressChanged(peerId, bytesReceived * 100 / bytesTotal);

    int finished = m_targets.size() - m_pendingDownloads.size();
    emit progressChanged((finished * 100 + bytesReceived * 100 / bytesTotal) / m_targets.size());
}

void QnDownloadUpdatesPeerTask::at_downloadReply_readyRead() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        NX_ASSERT(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    m_triesCount = 0;

    // it means the task was cancelled
    if (m_file.isNull()) {
        reply->deleteLater();
        return;
    }

    if (!readAllData(reply, m_file.data())) {
        NX_LOG(lit("Update: QnDownloadUpdatesPeerTask: No free space."), cl_logERROR);
        finishTask(NoFreeSpaceError);
    }
}
