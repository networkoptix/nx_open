#ifndef REST_UPDATE_PEER_TASK_H
#define REST_UPDATE_PEER_TASK_H

#include <update/task/network_peer_task.h>
#include <utils/common/system_information.h>
#include <core/resource/media_server_resource.h>

class QnRestUpdatePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        ParametersError,
        FileError,
        UploadError,
        InstallationError
    };

    explicit QnRestUpdatePeerTask(QObject *parent = 0);

    void setUpdateFiles(const QHash<QnSystemInformation, QString> &updateFiles);
    void setUpdateId(const QString &updateId);
    void setVersion(const QnSoftwareVersion &version);

    virtual void cancel() override;

signals:
    void peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id);

protected:
    virtual void doStart() override;

private:
    void installNextUpdate();
    void finishPeer();

private slots:
    void at_updateInstalled(int status, int handle);
    void at_resourceChanged(const QnResourcePtr &resource);
    void at_timer_timeout();
    void at_finished();

private:
    QString m_updateId;
    QnSoftwareVersion m_version;
    QHash<QnSystemInformation, QString> m_updateFiles;
    QMultiHash<QnSystemInformation, QnMediaServerResourcePtr> m_serverBySystemInformation;

    QByteArray m_currentData;
    QList<QnMediaServerResourcePtr> m_currentServers;
    QUuid m_targetId;
    QTimer *m_timer;
};

#endif // REST_UPDATE_PEER_TASK_H
