#ifndef NETWORK_PEER_TASK_H
#define NETWORK_PEER_TASK_H

#include <QtCore/QObject>

#include <utils/common/id.h>

class QnNetworkPeerTask : public QObject {
    Q_OBJECT
public:
    explicit QnNetworkPeerTask(QObject *parent = 0);
    ~QnNetworkPeerTask();

    QSet<QUuid> peers() const;
    void setPeers(const QSet<QUuid> &peers);

public slots:
    void start();
    void start(const QSet<QUuid> &peers);
    virtual void cancel() {}

signals:
    void finished(int errorCode, const QSet<QUuid> &failedPeers);
    void peerFinished(const QUuid &peerId);
    void peerProgressChanged(const QUuid &peerId, int progress);
    void progressChanged(int progress);

protected:
    void finish(int errorCode, const QSet<QUuid> &failedPeers = QSet<QUuid>());

    virtual void doStart() = 0;

private:
    QSet<QUuid> m_peers;
};

#endif // NETWORK_PEER_TASK_H
