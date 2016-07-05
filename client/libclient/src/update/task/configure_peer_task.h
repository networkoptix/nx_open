#pragma once

#include <update/task/network_peer_task.h>

struct QnConfigureReply;
struct QnModuleInformation;
class QnMergeSystemsTool;

class QnConfigurePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        AuthentificationFailed,
        UnknownError
    };

    explicit QnConfigurePeerTask(QObject *parent = 0);

    QString adminPassword() const;
    void setAdminPassword(const QString &adminPassword);

protected:
    virtual void doStart() override;

private slots:
    void at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int handle);

private:
    QnMergeSystemsTool *m_mergeTool;
    int m_error;
    QString m_adminPassword;

    QSet<QnUuid> m_pendingPeers;
    QSet<QnUuid> m_failedPeers;
    QHash<int, QnUuid> m_peerIdByHandle;
};
