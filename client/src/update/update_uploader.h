#ifndef UPDATE_UPLOADER_H
#define UPDATE_UPLOADER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <utils/common/uuid.h>


class QFile;
class QTimer;

class QnUpdateUploader : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        NoFreeSpace,
        UnknownError
    };

    QnUpdateUploader(QObject *parent = 0);

    bool uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnUuid> &peers);
    void markChunkUploaded(const QnUuid &peerId, qint64 offset);

    QString updateId() const;
    QSet<QnUuid> peers() const;

    void cancel();

signals:
    void peerProgressChanged(const QnUuid &peerId, int progress);
    void progressChanged(int progress);
    void finished(int errorCode);

private slots:
    void sendNextChunk();
    void at_updateManager_updateUploadProgress(const QString &updateId, const QnUuid &peerId, int chunks);
    void at_chunkTimer_timeout();

private:
    void cleanUp();

private:
    QString m_updateId;
    QSet<QnUuid> m_peers;
    QScopedPointer<QFile> m_updateFile;
    int m_chunkSize;
    int m_chunkCount;
    bool m_finalized;
    QSet<QnUuid> m_pendingPeers;

    QHash<QnUuid, int> m_progressById;
    QTimer *m_chunkTimer;
};

#endif // UPDATE_UPLOADER_H
