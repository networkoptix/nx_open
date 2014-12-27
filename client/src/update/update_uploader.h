#ifndef UPDATE_UPLOADER_H
#define UPDATE_UPLOADER_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <utils/common/uuid.h>
#include <core/resource/resource_fwd.h>


class QFile;
class QTimer;
struct QnUploadUpdateReply;

class QnUpdateUploader : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        NoFreeSpace,
        TimeoutError,
        UnknownError
    };

    QnUpdateUploader(QObject *parent = 0);

    bool uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnUuid> &peers);

    QString updateId() const;
    QSet<QnUuid> peers() const;

    void cancel();

signals:
    void peerProgressChanged(const QnUuid &peerId, int progress);
    void progressChanged(int progress);
    void finished(int errorCode, const QSet<QnUuid> &failedPeers);

private slots:
    void sendPreambule();
    void sendNextChunk();
    void at_updateManager_updateUploadProgress(const QString &updateId, const QnUuid &peerId, int chunks);
    void at_chunkTimer_timeout();
    void at_restReply_finished(int status, const QnUploadUpdateReply &reply, int handle);

private:
    void cleanUp();
    void handleUploadProgress(const QnUuid &peerId, qint64 chunks);

private:
    QString m_updateId;
    QSet<QnUuid> m_peers;
    QSet<QnUuid> m_restPeers;
    QList<QnMediaServerResourcePtr> m_restTargets;
    QHash<int, QnMediaServerResourcePtr> m_restRequsts;
    QScopedPointer<QFile> m_updateFile;
    int m_chunkSize;
    int m_chunkCount;
    bool m_finalized;
    QSet<QnUuid> m_pendingPeers;

    QHash<QnUuid, int> m_progressById;
    QTimer *m_chunkTimer;
};

#endif // UPDATE_UPLOADER_H
