#ifndef CHANGE_SYSTEM_NAME_PEER_TASK_H
#define CHANGE_SYSTEM_NAME_PEER_TASK_H

#include <utils/network_peer_task.h>

class QnChangeSystemNamePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    explicit QnChangeSystemNamePeerTask(QObject *parent = 0);

    QString systemName() const;
    void setSystemName(const QString &systemName);
    void setRebootPeers(bool rebootPeers);

protected:
    virtual void doStart() override;

private slots:
    void processReply(int status, int handle);

private:
    QString m_systemName;
    QHash<int, QnId> m_pendingPeers;
    QSet<QnId> m_failedPeers;
    bool m_rebootPeers;
};

#endif // CHANGE_SYSTEM_NAME_PEER_TASK_H
