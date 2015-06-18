#ifndef INSTALL_UPDATES_PEER_TASK_H
#define INSTALL_UPDATES_PEER_TASK_H

#include <core/resource/resource_fwd.h>
#include <update/task/network_peer_task.h>
#include <utils/common/software_version.h>
#include <utils/network/module_information.h>
#include <api/api_fwd.h>

class QTimer;
struct QnUploadUpdateReply;

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
    void at_installUpdateResponse(int status, const QnUploadUpdateReply &reply, int handle);

private:
    void finish(int errorCode, const QSet<QnUuid> &failedPeers = QSet<QnUuid>());

private:
    QString m_updateId;
    QnMediaServerResourcePtr m_ecServer;
    QnMediaServerConnectionPtr m_ecConnection;
    QnSoftwareVersion m_version;
    QSet<QnUuid> m_stoppingPeers;
    QSet<QnUuid> m_restartingPeers;
    QSet<QnUuid> m_pendingPeers;
    QTimer *m_checkTimer;
    QTimer *m_pingTimer;
    bool m_protoProblemDetected;
    QHash<int, QnMediaServerResourcePtr> m_serverByRequest;
};

#endif // INSTALL_UPDATES_PEER_TASK_H
