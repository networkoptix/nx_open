#include "network_peer_task.h"

QnNetworkPeerTask::QnNetworkPeerTask(QObject *parent) :
    QObject(parent)
{
}

QnNetworkPeerTask::~QnNetworkPeerTask() {}

void QnNetworkPeerTask::start(const QSet<QnId> &peers) {
    m_peers = peers;
    doStart();
}

QSet<QnId> QnNetworkPeerTask::peers() const {
    return m_peers;
}

void QnNetworkPeerTask::finish(int errorCode, const QSet<QnId> &failedPeers) {
    emit finished(errorCode, failedPeers);
}
