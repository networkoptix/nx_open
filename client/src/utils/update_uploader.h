#ifndef UPDATE_UPLOADER_H
#define UPDATE_UPLOADER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx_ec/ec_api.h>
#include <utils/common/id.h>

class QFile;

class QnUpdateUploader : public QObject {
    Q_OBJECT
public:
    QnUpdateUploader(QObject *parent = 0);

    bool uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnId> &peers);
    void markChunkUploaded(const QnId &peerId, qint64 offset);

    QString uploadId() const;
    QSet<QnId> peers() const;

    void cancel();

signals:
    void peerProgressChanged(const QnId &peerId, int progress);
    void progressChanged(int progress);
    void finished();
    void failed();

private slots:
    void sendNextChunk();
    void requestSendNextChunk();
    void finalize(const QnId &id = QnId());
    void continueSending();
    void pauseSending();

private:
    void chunkUploaded(int reqId, ec2::ErrorCode errorCode);
    void cleanUp();

private:
    QString m_updateId;
    QScopedPointer<QFile> m_updateFile;

    struct ChunkData {
        int index;
        QSet<QnId> peers;
    };

    qint64 m_size;
    int m_chunkCount;
    QSet<QnId> m_peers;
    int m_chunkSize;
    QList<ChunkData> m_chunks;
    QHash<QnId, int> m_leftChunksById;
    int m_leftChunks;
    int m_currentIndex;

    QSet<QnId> m_pendingFinalizations;

    bool m_pause;
};

#endif // UPDATE_UPLOADER_H
