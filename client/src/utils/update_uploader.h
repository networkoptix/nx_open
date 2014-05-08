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

    QString updateId() const;
    QSet<QnId> peers() const;

    void cancel();

signals:
    void peerProgressChanged(const QnId &peerId, int progress);
    void progressChanged(int progress);
    void finished();
    void failed();

private slots:
    void sendNextChunk();
    void at_updateManager_updateUploadProgress(const QString &updateId, const QnId &peerId, int chunks);

private:
    void chunkUploaded(int reqId, ec2::ErrorCode errorCode);
    void cleanUp();

private:
    QString m_updateId;
    QSet<QnId> m_peers;
    QScopedPointer<QFile> m_updateFile;
    int m_chunkSize;
    int m_chunkCount;

    QHash<QnId, int> m_progressById;
};

#endif // UPDATE_UPLOADER_H
