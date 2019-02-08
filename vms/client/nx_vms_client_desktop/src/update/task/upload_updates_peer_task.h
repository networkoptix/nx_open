#pragma once

#include <update/task/network_peer_task.h>

#include <nx/vms/api/data/system_information.h>

class QnUpdateUploader;

class QnUploadUpdatesPeerTask: public QnNetworkPeerTask
{
    Q_OBJECT

public:
    enum ErrorCode
    {
        NoError = 0,
        ConsistencyError,
        UploadError,
        TimeoutError,
        AuthenticationError,
        NoFreeSpaceError
    };

    explicit QnUploadUpdatesPeerTask(QObject* parent = nullptr);

    void setUploads(const QHash<nx::vms::api::SystemInformation, QString>& fileBySystemInformation);
    void setUpdateId(const QString& updateId);

protected:
    virtual void doStart() override;
    virtual void doCancel() override;

private:
    void uploadNextUpdate();

private slots:
    void at_uploader_finished(int errorCode, const QSet<QnUuid>& failedPeers);
    void at_uploader_progressChanged(int progress);

private:
    QString m_updateId;
    QHash<nx::vms::api::SystemInformation, QString> m_fileBySystemInformation;
    QMultiHash<nx::vms::api::SystemInformation, QnUuid> m_peersBySystemInformation;
    QList<nx::vms::api::SystemInformation> m_pendingUploads;
    QSet<QnUuid> m_finishedPeers;

    QnUpdateUploader* const m_uploader;
};
