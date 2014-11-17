#ifndef INSTALL_UPDATES_PEER_TASK_H
#define INSTALL_UPDATES_PEER_TASK_H

#include <core/resource/resource_fwd.h>
#include <update/task/network_peer_task.h>
#include <utils/common/software_version.h>
#include <utils/network/module_information.h>

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
    void at_checkTimer_timeout();
    void at_pingTimer_timeout();
    void at_gotModuleInformation(int status, const QList<QnModuleInformation> &modules, int handle);

private:
    void finish(int errorCode);

private:
    QString m_updateId;
    QnMediaServerResourcePtr m_ecServer;
    QnSoftwareVersion m_version;
    QSet<QnUuid> m_stoppingPeers;
    QSet<QnUuid> m_restartingPeers;
    QSet<QnUuid> m_pendingPeers;
    QTimer *m_checkTimer;
    QTimer *m_pingTimer;
};

#endif // INSTALL_UPDATES_PEER_TASK_H
