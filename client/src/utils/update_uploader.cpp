#include "update_uploader.h"

#include <QtCore/QFile>
#include <QtCore/QTimer>

#include <api/app_server_connection.h>

namespace {
    const int chunkSize = 1024 * 1024;
    const int chunkDelay = 10; // short delay between sendUpdateChunk commands

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
} // anonymous namespace

QnUpdateUploader::QnUpdateUploader(QObject *parent) :
    QObject(parent),
    m_chunkSize(chunkSize)
{
}

QString QnUpdateUploader::updateId() const {
    return m_updateId;
}

QSet<QUuid> QnUpdateUploader::peers() const {
    return m_peers;
}

void QnUpdateUploader::cancel() {
    cleanUp();
}

bool QnUpdateUploader::uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QUuid> &peers) {
    if (m_updateFile)
        return false;

    m_updateFile.reset(new QFile(fileName));
    if (!m_updateFile->open(QFile::ReadOnly)) {
        m_updateFile.reset();
        return false;
    }

    m_updateId = updateId;
    m_peers = peers;
    m_chunkCount = (m_updateFile->size() + m_chunkSize - 1) / m_chunkSize;

    m_progressById.clear();
    foreach (const QUuid &peerId, peers)
        m_progressById[peerId] = 0;

    connect(connection2()->getUpdatesManager().get(),   &ec2::AbstractUpdatesManager::updateUploadProgress,   this,   &QnUpdateUploader::at_updateManager_updateUploadProgress);
    sendNextChunk();
    return true;
}

void QnUpdateUploader::sendNextChunk() {
    if (m_updateFile.isNull())
        return;

    qint64 offset = m_updateFile->pos();
    QByteArray data = m_updateFile->read(m_chunkSize);

    connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, data, offset, m_peers,
                                                               this, &QnUpdateUploader::chunkUploaded);

    if (data.size() > 0 && m_updateFile->atEnd()) {
        // send tailing request just after the last chunk

        connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, QByteArray(), m_updateFile->size(), m_peers,
                                                                   this, &QnUpdateUploader::chunkUploaded);
    }
}

void QnUpdateUploader::chunkUploaded(int reqId, ec2::ErrorCode errorCode) {
    Q_UNUSED(reqId)

    if (m_updateFile.isNull())
        return;

    if (errorCode != ec2::ErrorCode::ok) {
        cleanUp();
        emit failed();
        return;
    }

    if (!m_updateFile->atEnd())
        QTimer::singleShot(chunkDelay, this, SLOT(sendNextChunk()));
}

void QnUpdateUploader::at_updateManager_updateUploadProgress(const QString &updateId, const QUuid &peerId, int chunks) {
    if (updateId != m_updateId)
        return;

    if (m_updateFile.isNull())
        return;

    auto it = m_progressById.find(peerId);
    if (it == m_progressById.end()) // it means we have already done upload for this peer
        return;

    int progress = 0;

    if (chunks < 0) {
        m_progressById.erase(it);

        if (chunks == ec2::AbstractUpdatesManager::Failed) {
            cleanUp();
            emit failed();
            return;
        } else {
            progress = 100;
        }
    } else {
        *it = progress = qMax(*it, chunks * 100 / m_chunkCount);
    }

    emit peerProgressChanged(peerId, progress);

    qint64 wholeProgress = (m_peers.size() - m_progressById.size()) * 100;
    foreach (int progress, m_progressById)
        wholeProgress += progress;
    emit progressChanged(wholeProgress / m_peers.size());

    if (m_progressById.isEmpty()) {
        cleanUp();
        emit finished();
    }
}

void QnUpdateUploader::cleanUp() {
    m_updateFile.reset();
    m_updateId.clear();
    connection2()->getUpdatesManager()->disconnect(this);
}
