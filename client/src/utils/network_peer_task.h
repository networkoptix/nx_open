#ifndef NETWORK_PEER_TASK_H
#define NETWORK_PEER_TASK_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnNetworkPeerTask : public QObject {
    Q_OBJECT
public:
    explicit QnNetworkPeerTask(QObject *parent = 0);
    ~QnNetworkPeerTask();

    void start(const QSet<QnId> &peers);

    QSet<QnId> peers() const;

signals:
    void finished(int errorCode, const QSet<QnId> &failedPeers);

protected:
    void finish(int errorCode, const QSet<QnId> &failedPeers = QSet<QnId>());

    virtual void doStart() = 0;

private:
    QSet<QnId> m_peers;
};

#endif // NETWORK_PEER_TASK_H
