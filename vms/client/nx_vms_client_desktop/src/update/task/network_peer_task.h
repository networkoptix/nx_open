#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <utils/common/id.h>

class QnNetworkPeerTask : public QObject, public QnConnectionContextAware
{
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
