#ifndef WAIT_COMPATIBLE_SERVERS_PEER_TASK_H
#define WAIT_COMPATIBLE_SERVERS_PEER_TASK_H

#include <update/task/network_peer_task.h>
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

protected:
    virtual void doStart() override;

private slots:
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);
    void at_timer_timeout();

private:
    void finishTask(int errorCode);

private:
    QSet<QnUuid> m_targets;

    QTimer *m_timer;
};

#endif // WAIT_COMPATIBLE_SERVERS_PEER_TASK_H
