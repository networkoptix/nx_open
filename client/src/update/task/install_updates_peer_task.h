#ifndef INSTALL_UPDATES_PEER_TASK_H
#define INSTALL_UPDATES_PEER_TASK_H

#include <core/resource/resource_fwd.h>
#include <update/task/network_peer_task.h>
#include <utils/common/software_version.h>

class QTimer;

class QnInstallUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        InstallationFailed
    };

    explicit QnInstallUpdatesPeerTask(QObject *parent = 0);

    void setUpdateId(const QString &updateId);
    void setVersion(const QnSoftwareVersion &version);

protected:
    virtual void doStart() override;

private slots:
    void at_resourceChanged(const QnResourcePtr &resource);
    void at_checkTimeout();

private:
    void finish(int errorCode);

private:
    QString m_updateId;
    QnSoftwareVersion m_version;
    QSet<QUuid> m_stoppingPeers;
    QSet<QUuid> m_restartingPeers;
    QSet<QUuid> m_pendingPeers;
    QTimer *m_checkTimer;
};

#endif // INSTALL_UPDATES_PEER_TASK_H
