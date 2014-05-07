#include "update_uploader.h"

#include <QtCore/QFile>
#include <QtCore/QTimer>

#include <api/app_server_connection.h>

namespace {
    const int chunkSize = 1024 * 1024;
    const int roundDelay = 500;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
} // anonymous namespace

QnUpdateUploader::QnUpdateUploader(QObject *parent) :
    QObject(parent),
    m_pause(false)
{
    m_chunkSize = chunkSize;
}

bool QnUpdateUploader::uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnId> &peers) {
    if (m_updateFile || !m_updateId.isEmpty())
        return false;

    m_updateFile.reset(new QFile(fileName));
    if (!m_updateFile->open(QFile::ReadOnly)) {
        m_updateFile.reset();
        return false;
    }

    m_updateId = updateId;
    m_peers = peers;
    m_size = m_updateFile->size();

    // now split file to chunks and start transfer
    m_chunkCount = (m_size + m_chunkSize - 1) / m_chunkSize;
    for (int i = 0; i < m_chunkCount; i++) {
        ChunkData chunk;
        chunk.index = i;
        chunk.peers = peers;
        m_chunks.append(chunk);
    }

    foreach (const QnId &id, peers)
        m_leftChunksById[id] = m_chunkCount;
    m_leftChunks = m_chunkCount * m_peers.size();

    m_currentIndex = 0;
    m_pendingFinalizations = peers;

    requestSendNextChunk();

    return true;
}

void QnUpdateUploader::markChunkUploaded(const QnId &peerId, qint64 offset) {
    if (offset == -1) {
        m_pendingFinalizations.remove(peerId);

        if (m_pendingFinalizations.isEmpty()) {
            cleanUp();
            emit finished();
        }
        return;
    }

    int index = offset / m_chunkSize;
    for (int i = 0; i < m_chunks.size(); i++) {
        if (m_chunks[i].index == index) {
            m_chunks[i].peers.remove(peerId);

            int &left = m_leftChunksById[peerId];
            if (left > 0)
                left--;

            if (m_leftChunks > 0)
                --m_leftChunks;

            emit peerProgressChanged(peerId, 100 * left / m_chunkCount);
            emit progressChanged(100 * m_leftChunks / (m_peers.size() * m_chunkCount));

            if (left == 0)
                finalize(peerId);

            break;
        }
    }
}

QString QnUpdateUploader::uploadId() const {
    return m_updateId;
}

QSet<QnId> QnUpdateUploader::peers() const {
    return m_peers;
}

void QnUpdateUploader::cancel() {
    cleanUp();
}

void QnUpdateUploader::sendNextChunk() {
    ChunkData *chunk = 0;
    for (;;) {
        if (m_currentIndex >= m_chunks.size()) {
            if (m_chunks.isEmpty()) {
                m_updateFile->close();
                m_updateFile.reset();
                return;
            } else {
                m_currentIndex = 0;
            }
        }

        chunk = &(m_chunks[m_currentIndex]);
        if (chunk->peers.isEmpty())
            m_chunks.removeAt(m_currentIndex);
        else
            break;
    }

    qint64 offset = chunk->index * m_chunkSize;
    m_updateFile->seek(offset);
    QByteArray data = m_updateFile->read(m_chunkSize);

    connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, data, offset, chunk->peers,
                                                               this, &QnUpdateUploader::chunkUploaded);

    m_currentIndex++;
    if (m_currentIndex == m_chunks.size())
        pauseSending();
}

void QnUpdateUploader::requestSendNextChunk() {
    if (m_pause || !m_updateFile)
        return;

    sendNextChunk();
}

void QnUpdateUploader::finalize(const QnId &id) {
    if (!id.isNull() && !m_pendingFinalizations.contains(id))
        return;

    connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, QByteArray(), m_size,
                                                               id.isNull() ? m_pendingFinalizations : (ec2::PeerList() << id),
                                                               this, [this](int, ec2::ErrorCode){});
}

void QnUpdateUploader::continueSending() {
    m_pause = false;
    requestSendNextChunk();
}

void QnUpdateUploader::pauseSending() {
    m_pause = true;
    QTimer::singleShot(roundDelay, this, SLOT(continueSending()));
}

void QnUpdateUploader::chunkUploaded(int reqId, ec2::ErrorCode errorCode) {
    Q_UNUSED(reqId)

    if (errorCode != ec2::ErrorCode::ok) {
        cleanUp();
        emit failed();
        return;
    }

    requestSendNextChunk();
}

void QnUpdateUploader::cleanUp() {
    m_chunks.clear();
    m_updateFile.reset();
    m_updateId.clear();
}
