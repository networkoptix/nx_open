#ifndef WAIT_COMPATIBLE_SERVERS_PEER_TASK_H
#define WAIT_COMPATIBLE_SERVERS_PEER_TASK_H

#include <utils/network_peer_task.h>
#include <core/resource/resource_fwd.h>

class QTimer;

class QnWaitCompatibleServersPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError,
        TimeoutError
    };

    explicit QnWaitCompatibleServersPeerTask(QObject *parent = 0);

    void setTargets(const QHash<QUuid, QUuid> &uuids);
    QHash<QUuid, QUuid> targets() const;

protected:
    virtual void doStart() override;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_timer_timeout();

private:
    void finishTask(int errorCode);

private:
    QHash<QUuid, QUuid> m_targets;
    QSet<QUuid> m_realTargets;

    QTimer *m_timer;
};

#endif // WAIT_COMPATIBLE_SERVERS_PEER_TASK_H
