#ifndef NETWORK_PEER_TASK_H
#define NETWORK_PEER_TASK_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnNetworkPeerTask : public QObject {
    Q_OBJECT
public:
    explicit QnNetworkPeerTask(QObject *parent = 0);
    ~QnNetworkPeerTask();

    QSet<QnUuid> peers() const;

    void start(const QSet<QnUuid> &peers);
    void cancel();

signals:
    void finished(int errorCode, const QSet<QnUuid> &failedPeers);
    void peerFinished(const QnUuid &peerId);
    void peerProgressChanged(const QnUuid &peerId, int progress);
    void progressChanged(int progress);

protected:
    void finish(int errorCode, const QSet<QnUuid> &failedPeers = QSet<QnUuid>());

    virtual void doStart() = 0;
    virtual void doCancel() {}

private:
    QSet<QnUuid> m_peers;
};

#endif // NETWORK_PEER_TASK_H
