#include "network_peer_task.h"

QnNetworkPeerTask::QnNetworkPeerTask(QObject *parent) :
    QObject(parent)
{
}

QnNetworkPeerTask::~QnNetworkPeerTask() {}

void QnNetworkPeerTask::start(const QSet<QnUuid> &peers) {
    m_peers = peers;
    doStart();
}

void QnNetworkPeerTask::cancel() {
    doCancel();
}

QSet<QnUuid> QnNetworkPeerTask::peers() const {
    return m_peers;
}

void QnNetworkPeerTask::finish(int errorCode, const QSet<QnUuid> &failedPeers) {
    emit finished(errorCode, failedPeers);
}
