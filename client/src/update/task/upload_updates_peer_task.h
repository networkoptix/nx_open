#ifndef UPLOAD_UPDATES_PEER_TASK_H
#define UPLOAD_UPDATES_PEER_TASK_H

#include <update/task/network_peer_task.h>
#include <utils/common/system_information.h>

class QnUpdateUploader;

class QnUploadUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        ConsistencyError,
        UploadError
    };

    explicit QnUploadUpdatesPeerTask(QObject *parent = 0);

    void setUploads(const QHash<QnSystemInformation, QString> &fileBySystemInformation);
    void setUpdateId(const QString &updateId);

    virtual void cancel() override;

protected:
    virtual void doStart() override;

private:
    void uploadNextUpdate();

private slots:
    void at_uploader_finished();
    void at_uploader_failed();
    void at_uploader_progressChanged(int progress);

private:
    QString m_updateId;
    QHash<QnSystemInformation, QString> m_fileBySystemInformation;
    QMultiHash<QnSystemInformation, QUuid> m_peersBySystemInformation;
    QList<QnSystemInformation> m_pendingUploads;
    QSet<QUuid> m_finishedPeers;

    QnUpdateUploader *m_uploader;
};

#endif // UPLOAD_UPDATES_PEER_TASK_H
