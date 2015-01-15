#ifndef CONFIGURE_PEER_TASK_H
#define CONFIGURE_PEER_TASK_H

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

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

protected:
    virtual void doStart() override;

private slots:
    void at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation, int handle);

private:
    QnMergeSystemsTool *m_mergeTool;
    int m_error;
    QString m_user;
    QString m_password;

    QSet<QnUuid> m_pendingPeers;
    QSet<QnUuid> m_failedPeers;
    QHash<int, QnUuid> m_peerIdByHandle;
};

#endif // CONFIGURE_PEER_TASK_H
